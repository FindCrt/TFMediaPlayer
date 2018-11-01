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
#include "FFmpegInternalDebug.h"
#include "TFStateObserver.hpp"

using namespace tfmpcore;


bool PlayController::connectAndOpenMedia(std::string mediaPath){
    
    this->mediaPath = mediaPath;
    
    av_register_all();
    avformat_network_init();
    
    fmtCtx = avformat_alloc_context();
    if (!fmtCtx) {
        return false;
    }
    
//    fmtCtx->interrupt_callback = {connectFail, this};
    int retval = avformat_open_input(&fmtCtx, mediaPath.c_str(), NULL, NULL);
    TFCheckRetvalAndGotoFail("avformat_open_input");
    
    if (strcmp(fmtCtx->filename, mediaPath.c_str()) != 0) {
        goto fail;
    }
    abortConnecting = false;
    
    //configure options to get faster
    retval = avformat_find_stream_info(fmtCtx, NULL);
    TFCheckRetvalAndGotoFail("avformat_find_stream_info");
    
    for (int i = 0; i<fmtCtx->nb_streams; i++) {
        
        AVMediaType type = fmtCtx->streams[i]->codecpar->codec_type;
        if (type == AVMEDIA_TYPE_VIDEO) {
#if EnableVTBDecode
            videoDecoder = new VTBDecoder(fmtCtx, i, type);
#else
            videoDecoder = new Decoder(fmtCtx, i, type);
#endif
            videoDecoder->name = "videoDecoder";
            videoDecoder->timebase = fmtCtx->streams[i]->time_base;
            videoStrem = i;
        }else if (type == AVMEDIA_TYPE_AUDIO){
            audioDecoder = new Decoder(fmtCtx, i, type);
            audioDecoder->name = "audioDecoder";
            audioStream = i;
        }else if (type == AVMEDIA_TYPE_SUBTITLE){
            subtitleDecoder = new Decoder(fmtCtx, i, type);
            subTitleStream = i;
        }
    }
    
    if (videoStrem < 0 && audioStream < 0) {
        goto fail;
    }
    
    if (videoDecoder && !videoDecoder->prepareDecode()) {
        goto fail;
    }
    
    if (audioDecoder && !audioDecoder->prepareDecode()) {
        goto fail;
    }
    
    if (subtitleDecoder && !subtitleDecoder->prepareDecode()) {
        goto fail;
    }
    
    displayer = new DisplayController();
    
    //audio format
    if (audioStream >= 0) resolveAudioStreamFormat();
    
    
    //check whether stream can display.
    if ((desiredDisplayMediaType & TFMP_MEDIA_TYPE_VIDEO) && videoDecoder != nullptr && displayVideoFrame == nullptr) {
        goto fail;
    }else{
        displayer->displayVideoFrame = displayVideoFrame;
    }
    
    displayer->displayContext = displayContext;
    if (videoStrem >= 0) {
        displayer->shareVideoBuffer = videoDecoder->sharedFrameBuffer();
        displayer->videoTimeBase = fmtCtx->streams[videoStrem]->time_base;
        
        AVRational frameRate = fmtCtx->streams[videoStrem]->avg_frame_rate;
        displayer->averageVideoDu = (double)frameRate.den/frameRate.num;
    }
    if (audioStream >= 0) {
        displayer->shareAudioBuffer = audioDecoder->sharedFrameBuffer();
        displayer->audioTimeBase = fmtCtx->streams[audioStream]->time_base;
        
        AVRational frameRate = fmtCtx->streams[videoStrem]->avg_frame_rate;
        displayer->averageVideoDu = (double)frameRate.den/frameRate.num;
    }
    
    calculateRealDisplayMediaType();
    setupSyncClock();
    
    duration = fmtCtx->duration/(double)AV_TIME_BASE;
    
    prapareOK = true;
    
    return true;
    
fail:
    avformat_close_input(&fmtCtx);
    avformat_free_context(fmtCtx);
    if (videoDecoder) free(videoDecoder);
    if (audioDecoder) free(audioDecoder);
    if (subtitleDecoder) free(subtitleDecoder);
    return false;
}

#pragma mark - controls

void PlayController::cancelConnecting(){
    abortConnecting = true;
}

void PlayController::play(){
    if (!prapareOK) {
        return;
    }
    printf("start Play: %s\n",mediaPath.c_str());
    readable = true;
    
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
        audioDecoder->sharedFrameBuffer()->addObserver(this, bufferEmptySize, false, videoFrameSizeNotified);
        audioDecoder->sharedFrameBuffer()->addObserver(this, playResumeSize, true, videoFrameSizeNotified);
    }else if(realDisplayMediaType & TFMP_MEDIA_TYPE_VIDEO){
//        videoDecoder->sharedFrameBuffer()->addObserver(this, bufferEmptySize, false, videoFrameSizeNotified);
//        videoDecoder->sharedFrameBuffer()->addObserver(this, playResumeSize, true, videoFrameSizeNotified);
    }
    
}


void PlayController::pause(bool flag){
    
    if (flag) {
        markTime = getCurrentTime();
    }
    
    paused = flag;
    
    //just change state of displaying, don't change state of reading.
    displayer->pause(flag);
    //TODO: resume readable if paused is false.
}

void PlayController::stop(){
    
    abortRequest = true;
    paused = false;
    
    //wait for read thread's end
    pthread_join(readThread, nullptr);
    
    //stop components
    if (videoDecoder) {
        videoDecoder->stopDecode();
    }
    if (audioDecoder) {
        audioDecoder->stopDecode();
    }
    if (subtitleDecoder) {
        subtitleDecoder->stopDecode();
    }
    displayer->stopDisplay();
    
    //free components
    if (videoDecoder) {
        free(videoDecoder);
        videoDecoder = nullptr;
    }
    if (audioDecoder) {
        free(audioDecoder);
        audioDecoder = nullptr;
    }
    if (subtitleDecoder) {
        free(subtitleDecoder);
        subtitleDecoder = nullptr;
    }
    free(displayer);
    displayer = nullptr;
    
    //free ffmpeg's resources
    if (fmtCtx) {
        avformat_close_input(&fmtCtx);
        avformat_free_context(fmtCtx);
    }
    
    //reset
    resetStatus();
    //notify others
    if (playStoped) {
        playStoped(this, 1);
    }
}

void PlayController::seekTo(double time){
    
    if (time >= duration) {
        time = duration-0.1;
    }
    
    markTime = time;
    seekPos = time;
    seeking = true;
    
    TFMPDLOG_C("seeking to %.6f\n",seekPos);
}

void PlayController::seekByForward(double interval){
    double currentTime = getCurrentTime();
    
    double seekTime = currentTime + interval;
    seekTo(seekTime);
}

void PlayController::bufferDone(){
    
    if (bufferingStateChanged) {
        bufferingStateChanged(this, false);
    }
}

void PlayController::resetStatus(){
    desiredDisplayMediaType = TFMP_MEDIA_TYPE_ALL_AVIABLE;
    realDisplayMediaType = TFMP_MEDIA_TYPE_NONE;
    
    videoStrem = -1;
    audioStream = -1;
    subTitleStream = -1;
    
    abortRequest = false;
    readable = false;
    checkingEnd = false;
    seeking = false;
    markTime = 0;
    
}

#pragma mark - properties

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
    
    double playTime = displayer->getPlayTime();
    if (seeking || paused || playTime < 0) {  //invalid time
        playTime = markTime;
    }
    
    return fmin(fmax(playTime, 0), duration);
}

#pragma mark - palying processes

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
    
    if (!(realDisplayMediaType & TFMP_MEDIA_TYPE_AUDIO) &&
        clockMajor == TFMP_SYNC_CLOCK_MAJOR_AUDIO) {
        clockMajor = TFMP_SYNC_CLOCK_MAJOR_VIDEO;
    }
    
    if (!(realDisplayMediaType & TFMP_MEDIA_TYPE_VIDEO) &&
        clockMajor == TFMP_SYNC_CLOCK_MAJOR_VIDEO){
        clockMajor = TFMP_SYNC_CLOCK_MAJOR_AUDIO;
    }
    
    displayer->clockMajor = clockMajor;
}

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
    displayer->setAudioDesc(negotiateAdoptedPlayAudioDesc(sourceDesc));
}

void PlayController::startReadingFrames(){
    pthread_create(&readThread, nullptr, readFrame, this);
}

void * PlayController::readFrame(void *context){
    
    PlayController *controller = (PlayController *)context;
    
    AVPacket *packet = nullptr;
    
    bool endFile = false;
    
    while (!controller->abortRequest) {
        
        if (controller->seeking) {
            myStateObserver.mark("read_frame", 1);
            controller->pause(true);
            int retval = avformat_seek_file(controller->fmtCtx, -1, INT64_MIN, controller->seekPos*AV_TIME_BASE, INT64_MAX, 0);
            myStateObserver.mark("read_frame", 2);
            if (retval == 0) {
                if (controller->audioDecoder) {
                    controller->audioDecoder->serial++;
                }
                if (controller->videoDecoder) {
                    controller->videoDecoder->serial++;
                }
                controller->displayer->serial++;
            }
            myStateObserver.mark("read_frame", 3);
            controller->seeking = false;
            controller->seekingEndNotify(controller);
            controller->pause(false);
        }
        
        packet = av_packet_alloc();
        myStateObserver.mark("read_frame", 7);
        int retval = av_read_frame(controller->fmtCtx, packet);
        myStateObserver.mark("read_frame", 8);

        if(retval < 0){
            myStateObserver.mark("read_frame", 9);
            if (retval == AVERROR_EOF) {
                endFile = true;
                
                controller->startCheckPlayFinish();
                myStateObserver.mark("read_frame", 10);
                av_usleep(100); //等下之后的处理，可能还会继续seek
            }else{
                myStateObserver.mark("read_frame", 11);
                av_packet_free(&packet);
                continue;
            }
        }
        
        if ((controller->realDisplayMediaType & TFMP_MEDIA_TYPE_VIDEO) &&
            packet->stream_index == controller->videoStrem) {
            myStateObserver.mark("read_frame", 4);
            controller->videoDecoder->insertPacket(packet);
            TFMPDLOG_C("[read] %.6f, %d\n",packet->pts*av_q2d(controller->videoDecoder->timebase), controller->videoDecoder->serial);
            
        }else if ((controller->realDisplayMediaType & TFMP_MEDIA_TYPE_AUDIO) &&
                  packet->stream_index == controller->audioStream){
            myStateObserver.mark("read_frame", 5);
            controller->audioDecoder->insertPacket(packet);
            
            
        }else if ((controller->realDisplayMediaType & TFMP_MEDIA_TYPE_SUBTITLE) &&
                  packet->stream_index == controller->subTitleStream){
            myStateObserver.mark("read_frame", 6);
            controller->subtitleDecoder->insertPacket(packet);
        }
        
        myStateObserver.timeMark("read frame time");
        
    }
    
    TFMPCondSignal(controller->waitLoopCond, controller->waitLoopMutex);
    
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


bool tfmpcore::videoFrameSizeNotified(RecycleBuffer<TFMPFrame *> *buffer, int curSize, bool isGreater,void *observer){
    
    PlayController *controller = (PlayController *)observer;
    
    if (curSize <= bufferEmptySize) {
        
        if (controller->checkingEnd){
            TFMPCondSignal(controller->read_cond, controller->read_mutex);
            
            pthread_create(&controller->signalThread, nullptr, PlayController::signalPlayFinished, controller);
            pthread_detach(controller->signalThread);
        }else{
            //buffer has ran out.We must stop playing until buffer is full again.
            if (controller->bufferingStateChanged) {
                controller->bufferingStateChanged(controller, true);
            }
        }

    }else if (curSize >= playResumeSize){
        
        controller->bufferDone();
        
    }

    return false;
}
