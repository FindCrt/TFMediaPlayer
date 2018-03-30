//
//  PlayController.cpp
//  TFMediaPlayer
//
//  Created by shiwei on 17/12/25.
//  Copyright © 2017年 shiwei. All rights reserved.
//

#include "PlayController.hpp"
#include "TFMPDebugFuncs.h"
#include "TFMPUtilities.h"
#if DEBUG
#include "FFmpegInternalDebug.h"
#endif

using namespace tfmpcore;



bool PlayController::connectAndOpenMedia(std::string mediaPath){
    
    this->mediaPath = mediaPath;
    
    av_register_all();
    avformat_network_init();
    
    fmtCtx = avformat_alloc_context();
    int retval = avformat_open_input(&fmtCtx, mediaPath.c_str(), NULL, NULL);
    TFCheckRetvalAndReturnFalse("avformat_open_input");
    
    //configure options to get faster
    retval = avformat_find_stream_info(fmtCtx, NULL);
    TFCheckRetvalAndReturnFalse("avformat_find_stream_info");
    
    for (int i = 0; i<fmtCtx->nb_streams; i++) {
        
        AVMediaType type = fmtCtx->streams[i]->codecpar->codec_type;
        if (type == AVMEDIA_TYPE_VIDEO) {
            videoDecoder = new Decoder(fmtCtx, i, type);
            videoDecoder->mediaTimeFilter = new MediaTimeFilter(fmtCtx->streams[i]->time_base);
            videoStrem = i;
        }else if (type == AVMEDIA_TYPE_AUDIO){
            audioDecoder = new Decoder(fmtCtx, i, type);
            audioDecoder->mediaTimeFilter = new MediaTimeFilter(fmtCtx->streams[i]->time_base);
            audioStream = i;
        }else if (type == AVMEDIA_TYPE_SUBTITLE){
            subtitleDecoder = new Decoder(fmtCtx, i, type);
            subTitleStream = i;
        }
    }
    
    if (videoStrem < 0 && audioStream < 0) {
        return false;
    }
    
    if (videoDecoder && !videoDecoder->prepareDecode()) {
        return false;
    }
    
    if (audioDecoder && !audioDecoder->prepareDecode()) {
        return false;
    }
    
    if (subtitleDecoder && !subtitleDecoder->prepareDecode()) {
        return false;
    }
    
    displayer = new DisplayController();
    
    //audio format
    if (audioStream >= 0) resolveAudioStreamFormat();
    
    
    //check whether stream can display.
    if ((desiredDisplayMediaType & TFMP_MEDIA_TYPE_VIDEO) && videoDecoder != nullptr && displayVideoFrame == nullptr) {
        return false;
    }else{
        displayer->displayVideoFrame = displayVideoFrame;
    }
    
    displayer->displayContext = displayContext;
    if (videoStrem >= 0) displayer->shareVideoBuffer = videoDecoder->sharedFrameBuffer();
    if (audioStream >= 0) displayer->shareAudioBuffer = audioDecoder->sharedFrameBuffer();
    
    if (videoStrem >= 0) displayer->videoTimeBase = fmtCtx->streams[videoStrem]->time_base;
    if (audioStream >= 0) displayer->audioTimeBase = fmtCtx->streams[audioStream]->time_base;
    
    calculateRealDisplayMediaType();
    setupSyncClock();
    
    displayer->checkValidFrameCallback = [this](){
        if (seeking) {
            seeking = false;
        }
        
        TFMPDLOG_C("checkValidFrameCallback, seeking:%s\n",seeking?"true":"false");
    };
    
    duration = fmtCtx->duration/(double)AV_TIME_BASE;
    
    prapareOK = true;
    
    if (connectCompleted != nullptr) {
        connectCompleted(this);
    }
    
    return true;
}

void PlayController::play(){
    if (!prapareOK) {
        return;
    }
    printf("start Play: %s\n",mediaPath.c_str());
    shouldRead = true;
    
    startReadingFrames();
    if (videoDecoder) {
        videoDecoder->startDecode();
    }
    if (audioDecoder) {
        audioDecoder->startDecode();
    }
    if (subtitleDecoder) {
        subtitleDecoder->startDecode();
    }
    
    displayer->startDisplay();
    
    
    //observe the exhaustion of buffers.
    Decoder *checkDecoder = nullptr;
    
    if (realDisplayMediaType & TFMP_MEDIA_TYPE_AUDIO) {
        checkDecoder = audioDecoder;
    }else if(realDisplayMediaType & TFMP_MEDIA_TYPE_VIDEO){
        checkDecoder = videoDecoder;
    }
    checkDecoder->sharedFrameBuffer()->addObserver(this, bufferEmptySize, false, videoFrameSizeNotified);
    checkDecoder->sharedFrameBuffer()->addObserver(this, playResumeSize, true, videoFrameSizeNotified);
}

void PlayController::pause(bool flag){
    paused = flag;
    displayer->pause(flag);
}

void PlayController::stop(){
    
    shouldRead = false;
    
    //displayer
    displayer->stopDisplay();
    
    //decodes
    if (videoDecoder) {
        videoDecoder->stopDecode();
    }
    if (audioDecoder) {
        audioDecoder->stopDecode();
    }
    if (subtitleDecoder) {
        subtitleDecoder->stopDecode();
    }
    
    freeResources();
    
    desiredDisplayMediaType = TFMP_MEDIA_TYPE_ALL_AVIABLE;
    realDisplayMediaType = TFMP_MEDIA_TYPE_NONE;
    
    videoStrem = -1;
    audioStream = -1;
    subTitleStream = -1;

    TFMPDLOG_C("player stoped!\n");
}

void PlayController::seekTo(double time){
    
    TFMPDLOG_C("seekTo: %d:%d\n",(int)time/60,(int)time%60);
    
    /* 
     1. turn off inlet
     2. flush all buffers 
     3. turn off outlet 
     4. seek stream to new position 
     5. turn on inlet
     6. turn on outlet when the pool fill to a certain size.
     */
    
    if (time > duration) {
        time = duration-0.1;
    }
    
    checkingEnd = false;
    seeking = true;
    seekingTime = time;
    
    //Flushing all old frames and packets. Firstly, stop reading new packets.
    //1. turn off inlet
    paused = true;
    
    //2. flush all buffers
    if (videoDecoder) {
        videoDecoder->flush();
    }
    if (audioDecoder) {
        audioDecoder->flush();
    }
    if (subtitleDecoder) {
        subtitleDecoder->flush();
    }
    
    displayer->flush();
    
    //3. enable mediaTimeFilter to filter unqualified frames whose pts is earlier than seeking time.
    if (videoDecoder) {
        videoDecoder->mediaTimeFilter->enable = true;
        videoDecoder->mediaTimeFilter->minMediaTime = seekingTime;
    }
    if (audioDecoder) {
        audioDecoder->mediaTimeFilter->enable = true;
        audioDecoder->mediaTimeFilter->minMediaTime = seekingTime;
    }
    
    //3. turn off outlet
    displayer->pause(true);
    
    TFMPDLOG_C("flush all end!\n");
    
    //4. seek stream to new position
    if (videoStrem >= 0) {
        av_seek_frame(fmtCtx, videoStrem, time/av_q2d(fmtCtx->streams[videoStrem]->time_base), AVSEEK_FLAG_BACKWARD);
    }else if (audioStream >= 0){
        av_seek_frame(fmtCtx, audioStream, time/av_q2d(fmtCtx->streams[audioStream]->time_base), AVSEEK_FLAG_BACKWARD);
    }
    
    //5. turn on inlet
    paused = false;
    pthread_cond_signal(&pause_cond);
    
    TFMPDLOG_C("seek end! %.3f\n",time);
}

void PlayController::seekByForward(double interval){
    double currentTime = getCurrentTime();
    
    double seekTime = currentTime + interval;
    seekTo(seekTime);
}

void PlayController::seekingEnd(){
    
    if (seeking) {
        seeking = false;
        
        if (seekingEndNotify) {
            seekingEndNotify(this);
        }
    }
}

void PlayController::freeResources(){
    
    //decodes
    if (videoDecoder) {
        videoDecoder->freeResources();
        free(videoDecoder);
        videoDecoder = nullptr;
        TFMPDLOG_C("videoDecoder null\n");
    }
    if (audioDecoder) {
        audioDecoder->freeResources();
        free(audioDecoder);
        audioDecoder = nullptr;
        TFMPDLOG_C("audioDecoder null\n");
    }
    if (subtitleDecoder) {
        subtitleDecoder->freeResources();
        free(subtitleDecoder);
        subtitleDecoder = nullptr;
    }
    
    displayer->freeResources();
    
    //ffmpeg
    if (fmtCtx) avformat_free_context(fmtCtx);
}

/***** properties *****/

void PlayController::calculateRealDisplayMediaType(){
    int realType = 0;
    if ((desiredDisplayMediaType & TFMP_MEDIA_TYPE_VIDEO) && (videoStrem >= 0)) {
        realType |= TFMP_MEDIA_TYPE_VIDEO;
    }
    if ((desiredDisplayMediaType & TFMP_MEDIA_TYPE_AUDIO) && (audioStream >= 0)) {
        realType |= TFMP_MEDIA_TYPE_AUDIO;
    }
    if ((desiredDisplayMediaType & TFMP_MEDIA_TYPE_SUBTITLE) && (subTitleStream >= 0)) {
        realType |= TFMP_MEDIA_TYPE_SUBTITLE;
    }
    
    realDisplayMediaType = (TFMPMediaType)realType;
    displayer->displayMediaType = (TFMPMediaType)realType;
}

void PlayController::setDesiredDisplayMediaType(TFMPMediaType desiredDisplayMediaType){
    this->desiredDisplayMediaType = desiredDisplayMediaType;
    if (prapareOK) {
        calculateRealDisplayMediaType();
        
        //media type may changed, sync clock need to change.
        setupSyncClock();
    }
}

void PlayController::setupSyncClock(){
    if (!(realDisplayMediaType & TFMP_MEDIA_TYPE_AUDIO) && isAudioMajor) isAudioMajor = false;
    if (!(realDisplayMediaType & TFMP_MEDIA_TYPE_VIDEO) && !isAudioMajor) isAudioMajor = true;
    
    if (displayer->syncClock) {
        displayer->syncClock->isAudioMajor = isAudioMajor;
    }else{
        displayer->syncClock = new SyncClock(isAudioMajor);
    }
    
}

TFMPFillAudioBufferStruct PlayController::getFillAudioBufferStruct(){
    return displayer->getFillAudioBufferStruct();
}
DisplayController *PlayController::getDisplayer(){
    return displayer;
}

double PlayController::getDuration(){
    return duration;
}

double PlayController::getCurrentTime(){
    if (seeking) {
        return seekingTime;
    }else{
        return fmin(displayer->getLastPlayTime(),duration);
    }
}

/***** private ******/

void PlayController::resolveAudioStreamFormat(){
    
    auto codecpar = fmtCtx->streams[audioStream]->codecpar;
    
    TFMPAudioStreamDescription sourceDesc;
    sourceDesc.sampleRate = codecpar->sample_rate;
    
    AVSampleFormat fmt = (AVSampleFormat)codecpar->format;
    
    sourceDesc.formatFlags = formatFlagsFromFFmpegAudioFormat(fmt);
    sourceDesc.bitsPerChannel = bitPerChannelFromFFmpegAudioFormat(fmt);
    
    sourceDesc.channelsPerFrame = codecpar->channels;
    sourceDesc.ffmpeg_channel_layout = codecpar->channel_layout;
    
    //resample source audio to real-play audio format.
    auto adoptedAudioDesc = negotiateAdoptedPlayAudioDesc(sourceDesc);
    
    auto audioResampler = new AudioResampler();
    audioResampler->adoptedAudioDesc = adoptedAudioDesc;
    displayer->setAudioResampler(audioResampler);
}

void PlayController::startReadingFrames(){
    pthread_create(&readThread, nullptr, readFrame, this);
    pthread_detach(readThread);
}

void * PlayController::readFrame(void *context){
    
    PlayController *controller = (PlayController *)context;
    
    AVPacket *packet = av_packet_alloc();
    
    bool endFile = false;
    
    while (controller->shouldRead) {
        
        if (controller->paused) {
            TFMPDLOG_C("read paused!\n");
            pthread_mutex_lock(&controller->pause_mutex);
            pthread_cond_wait(&controller->pause_cond, &controller->pause_mutex);
            pthread_mutex_unlock(&controller->pause_mutex);
        }
        
        int retval = av_read_frame(controller->fmtCtx, packet);

        if(retval < 0){
            if (retval == AVERROR_EOF) {
                endFile = true;
                
                controller->startCheckPlayFinish();
                
                pthread_mutex_lock(&controller->pause_mutex);
                pthread_cond_wait(&controller->pause_cond, &controller->pause_mutex);
                pthread_mutex_unlock(&controller->pause_mutex);
//                break;
            }else{
                continue;
            }
        }
        
        TFMPDLOG_C("\n\nread frame[%s]: %lld,%.3f\n",packet->stream_index==0?"video":"audio",packet->pts, packet->pts*av_q2d(controller->fmtCtx->streams[packet->stream_index]->time_base));
        
        if ((controller->realDisplayMediaType & TFMP_MEDIA_TYPE_VIDEO) &&
            packet->stream_index == controller->videoStrem) {
            
            controller->videoDecoder->decodePacket(packet);
            
        }else if ((controller->realDisplayMediaType & TFMP_MEDIA_TYPE_AUDIO) &&
                  packet->stream_index == controller->audioStream){
            
            controller->audioDecoder->decodePacket(packet);
            
        }else if ((controller->realDisplayMediaType & TFMP_MEDIA_TYPE_SUBTITLE) &&
                  packet->stream_index == controller->subTitleStream){
            
            controller->subtitleDecoder->decodePacket(packet);
        }
        
        av_packet_unref(packet);
    }
    
    TFMPDLOG_C("readFrame thread end!\n");
    
    return 0;
}

/** file has reach the end, if the data in packet buffer and frame buffer are used, all resources is showed then now it's need to stop.*/
void PlayController::startCheckPlayFinish(){
    
    //start to observe frame buffer's size. When the size is less than 1, in other words the buffer is empty, it's really time when video stops.
    if (!checkingEnd) {
        checkingEnd = true;
    }
}

void *PlayController::signalPlayFinished(void *context){
    
    PlayController *controller = (PlayController *)context;
    if (controller->playStoped) {
        controller->playStoped(controller, 0);
    }
    
    return 0;
}

#pragma mark -

bool tfmpcore::videoFrameSizeNotified(RecycleBuffer<AVFrame *> *buffer, int curSize, bool isGreater,void *observer){
    
    PlayController *controller = (PlayController *)observer;
    
    if (curSize <= bufferEmptySize) {
        
        if (controller->checkingEnd){
            pthread_cond_signal(&controller->pause_cond);
            
            pthread_create(&controller->signalThread, nullptr, PlayController::signalPlayFinished, controller);
            pthread_detach(controller->signalThread);
        }else{
            //buffer has ran out.We must stop playing until buffer is full again.
            controller->displayer->pause(true);
            TFMPDLOG_C("buffer runs out, stop playing\n");
        }

    }else if (curSize >= playResumeSize){
        
        if (buffer == controller->videoDecoder->sharedFrameBuffer()) {
            TFMPDLOG_C("video: frame buffer size has be greater than 20\n");
        }else{
            TFMPDLOG_C("audio: frame buffer size has be greater than 20\n");
        }
        
        controller->displayer->pause(false);
        controller->seekingEnd();
        
    }

    return false;
}
