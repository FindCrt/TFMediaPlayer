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

typedef struct{
    PlayController *playController;
    double seekTime;
}TFMPSeekOpParams;

//int PlayController::connectFail(void *opaque){
//    TFMPDLOG_C("connectFail ");
//    PlayController *controller = (PlayController*)opaque;
//    if (controller->abortConnecting) {
//        return AVERROR_EOF;
//    }
//    return 0;
//}

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
            videoDecoder = new Decoder(fmtCtx, i, type);
            videoDecoder->name = "videoDecoder";
            videoDecoder->mediaTimeFilter = new MediaTimeFilter(fmtCtx->streams[i]->time_base);
            videoStrem = i;
        }else if (type == AVMEDIA_TYPE_AUDIO){
            audioDecoder = new Decoder(fmtCtx, i, type);
            audioDecoder->name = "audioDecoder";
            audioDecoder->mediaTimeFilter = new MediaTimeFilter(fmtCtx->streams[i]->time_base);
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
#if DEBUG
        videoDecoder->timebase = displayer->videoTimeBase;
#endif
    }
    if (audioStream >= 0) {
        displayer->shareAudioBuffer = audioDecoder->sharedFrameBuffer();
        displayer->audioTimeBase = fmtCtx->streams[audioStream]->time_base;
        
#if DEBUG
        audioDecoder->timebase = displayer->audioTimeBase;
#endif
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
        checkDecoder = audioDecoder;
    }else if(realDisplayMediaType & TFMP_MEDIA_TYPE_VIDEO){
        checkDecoder = videoDecoder;
    }
    checkDecoder->sharedFrameBuffer()->addObserver(this, bufferEmptySize, false, videoFrameSizeNotified);
    checkDecoder->sharedFrameBuffer()->addObserver(this, playResumeSize, true, videoFrameSizeNotified);
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
    
    stoping = true;
    paused = false;
    
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
    
    pthread_create(&freeThread, nullptr, freeResources, this);
}

void *PlayController::seekOperation(void *context){
    TFMPSeekOpParams *params = (TFMPSeekOpParams *)context;
    double time = params->seekTime;
    PlayController *playController = params->playController;
    
    if (time > playController->duration) {
        time = playController->duration-0.1;
    }
    
    playController->checkingEnd = false;
    playController->prepareForSeeking = true;
    playController->seeking = true;
    playController->markTime = time;
    
    
    
    //Flushing all old frames and packets. Firstly, stop reading new packets.
    //1. turn off inlet
    if (playController->videoDecoder) {
        playController->videoDecoder->activeBlock(false);
    }
    if (playController->audioDecoder) {
        playController->audioDecoder->activeBlock(false);
    }
    if (playController->subtitleDecoder) {
        playController->subtitleDecoder->activeBlock(false);
    }
    
    playController->readable = false;
    pthread_mutex_lock(&playController->waitLoopMutex);
    if (playController->reading) {
        pthread_cond_wait(&playController->waitLoopCond, &playController->waitLoopMutex);
    }
    pthread_mutex_unlock(&playController->waitLoopMutex);
    
    //2. flush all buffers
    if (playController->videoDecoder) {
        playController->videoDecoder->flush();
    }
    if (playController->audioDecoder) {
        playController->audioDecoder->flush();
    }
    if (playController->subtitleDecoder) {
        playController->subtitleDecoder->flush();
    }
    
    playController->displayer->flush();
    
    //3. enable mediaTimeFilter to filter unqualified frames whose pts is earlier than seeking time.
    if (playController->videoDecoder) {
        playController->videoDecoder->mediaTimeFilter->enable = true;
        playController->videoDecoder->mediaTimeFilter->minMediaTime = time;
    }
    if (playController->audioDecoder) {
        playController->audioDecoder->mediaTimeFilter->enable = true;
        playController->audioDecoder->mediaTimeFilter->minMediaTime = time;
    }
    
    //3. turn off outlet
    if (playController->videoDecoder) {
        playController->videoDecoder->activeBlock(true);
    }
    if (playController->audioDecoder) {
        playController->audioDecoder->activeBlock(true);
    }
    if (playController->subtitleDecoder) {
        playController->subtitleDecoder->activeBlock(true);
    }
    playController->displayer->pause(true);
    playController->displayer->resetPlayTime();
    
    
    
    //4. seek stream to new position
    int retval = -1;
    if (playController->videoStrem >= 0) {
        retval = av_seek_frame(playController->fmtCtx, playController->videoStrem, time/av_q2d(playController->fmtCtx->streams[playController->videoStrem]->time_base), AVSEEK_FLAG_BACKWARD);
        TFCheckRetval("seek video");
    }else if (playController->audioStream >= 0){
        retval = av_seek_frame(playController->fmtCtx, playController->audioStream, time/av_q2d(playController->fmtCtx->streams[playController->audioStream]->time_base), AVSEEK_FLAG_BACKWARD);
        TFCheckRetval("seek audio");
    }
    
    if (retval < 0) { //seek failed
        if (playController->audioDecoder) {
            playController->audioDecoder->mediaTimeFilter->enable = false;
        }
        if (playController->videoDecoder) {
            playController->videoDecoder->mediaTimeFilter->enable = false;
        }
        
        playController->seeking = false;
        playController->displayer->pause(false);
        
        if (playController->seekingEndNotify) {
            playController->seekingEndNotify(playController);
        }
    }
    
    //5. turn on inlet
    playController->readable = true;
    TFMPCondSignal(playController->read_cond, playController->read_mutex);
    
    playController->prepareForSeeking = false;
    playController->displayer->pause(false);
    
    
    free(context);
    
    
    return 0;
}

void PlayController::seekTo(double time){
    
    auto param = new TFMPSeekOpParams();
    param->playController = this;
    param->seekTime = time;
    
    pthread_create(&seekThread, nullptr, seekOperation, param);
    pthread_detach(seekThread);
}

void PlayController::seekByForward(double interval){
    double currentTime = getCurrentTime();
    
    double seekTime = currentTime + interval;
    seekTo(seekTime);
}

void PlayController::bufferDone(){
    
    if (prepareForSeeking) {
        return;
    }
    
    if (bufferingStateChanged) {
        bufferingStateChanged(this, false);
    }
    
    if (seeking) {
        seeking = false;
        
        if (seekingEndNotify) {
            seekingEndNotify(this);
        }
    }
}

void *PlayController::freeResources(void *context){
    
    PlayController *playController = (PlayController *)context;
    
    //unblock pipline
    if (playController->videoDecoder) {
        playController->videoDecoder->activeBlock(false);
    }
    if (playController->audioDecoder) {
        playController->audioDecoder->activeBlock(false);
    }
    if (playController->subtitleDecoder) {
        playController->subtitleDecoder->activeBlock(false);
    }
    
    //read thread
    myStateObserver.labelMark("freeResources", "read thread");
    TFMPCondSignal(playController->read_cond, playController->read_mutex);
    pthread_mutex_lock(&playController->waitLoopMutex);
    if (playController->reading) {
        pthread_cond_wait(&playController->waitLoopCond, &playController->waitLoopMutex);
    }
    pthread_mutex_unlock(&playController->waitLoopMutex);
    
    //decodes
    if (playController->videoDecoder) {
        myStateObserver.labelMark("freeResources", "videoDecoder");
        playController->videoDecoder->freeResources();
        free(playController->videoDecoder);
        playController->videoDecoder = nullptr;
    }
    if (playController->audioDecoder) {
        myStateObserver.labelMark("freeResources", "audioDecoder");
        playController->audioDecoder->freeResources();
        free(playController->audioDecoder);
        playController->audioDecoder = nullptr;
    }
    if (playController->subtitleDecoder) {
        playController->subtitleDecoder->freeResources();
        free(playController->subtitleDecoder);
        playController->subtitleDecoder = nullptr;
    }
    
    
    
    myStateObserver.labelMark("freeResources", "display");
    playController->displayer->freeResources();
    
    myStateObserver.labelMark("freeResources", "ffmpeg");
    //ffmpeg
    if (playController->fmtCtx) {
        avformat_close_input(&playController->fmtCtx);
        avformat_free_context(playController->fmtCtx);
    }
    
    playController->resetStatus();
    
    if (playController->playStoped) {
        playController->playStoped(playController, 1);
    }
    
    return 0;
}

void PlayController::resetStatus(){
    desiredDisplayMediaType = TFMP_MEDIA_TYPE_ALL_AVIABLE;
    realDisplayMediaType = TFMP_MEDIA_TYPE_NONE;
    
    videoStrem = -1;
    audioStream = -1;
    subTitleStream = -1;
    
    stoping = false;
    readable = false;
    checkingEnd = false;
    seeking = false;
    prepareForSeeking = false;
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
    if (!(realDisplayMediaType & TFMP_MEDIA_TYPE_AUDIO) && isAudioMajor) isAudioMajor = false;
    if (!(realDisplayMediaType & TFMP_MEDIA_TYPE_VIDEO) && !isAudioMajor) isAudioMajor = true;
    
    if (displayer->syncClock) {
        displayer->syncClock->isAudioMajor = isAudioMajor;
    }else{
        displayer->syncClock = new SyncClock(isAudioMajor);
    }
    
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
    controller->reading = true;
    
    while (!controller->stoping) {
        myStateObserver.mark("reading", 1);
        if (!controller->readable) {
            
            controller->reading = false;
            TFMPCondSignal(controller->waitLoopCond, controller->waitLoopMutex);
            myStateObserver.mark("reading", 2);
            
            pthread_mutex_lock(&controller->read_mutex);
            if (!controller->readable) {
                pthread_cond_wait(&controller->read_cond, &controller->read_mutex);
                if (controller->stoping) {
                    pthread_mutex_unlock(&controller->read_mutex);
                    break;
                }
                myStateObserver.mark("reading", 3);
            }
            pthread_mutex_unlock(&controller->read_mutex);
            controller->reading = true;
        }
        
        myStateObserver.mark("reading", 5);
        int retval = av_read_frame(controller->fmtCtx, packet);

        if(retval < 0){
            if (retval == AVERROR_EOF) {
                endFile = true;
                
                controller->startCheckPlayFinish();
                myStateObserver.mark("reading", 6);
                TFMPCondWait(controller->read_cond, controller->read_mutex)
            }else{
                continue;
            }
        }
        myStateObserver.mark("reading", 7);
        
        if ((controller->realDisplayMediaType & TFMP_MEDIA_TYPE_VIDEO) &&
            packet->stream_index == controller->videoStrem) {
            
            controller->videoDecoder->decodePacket(packet);
            myStateObserver.timeMark("video frame in");
            
        }else if ((controller->realDisplayMediaType & TFMP_MEDIA_TYPE_AUDIO) &&
                  packet->stream_index == controller->audioStream){
            
            controller->audioDecoder->decodePacket(packet);
            myStateObserver.timeMark("audio frame in");
            
        }else if ((controller->realDisplayMediaType & TFMP_MEDIA_TYPE_SUBTITLE) &&
                  packet->stream_index == controller->subTitleStream){
            
            controller->subtitleDecoder->decodePacket(packet);
        }
        myStateObserver.mark("reading", 8);
        av_packet_unref(packet);
    }
    myStateObserver.mark("reading", 9);
    
    controller->reading = false;
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

bool tfmpcore::videoFrameSizeNotified(RecycleBuffer<AVFrame *> *buffer, int curSize, bool isGreater,void *observer){
    
    PlayController *controller = (PlayController *)observer;
    
    if (curSize <= bufferEmptySize) {
        
        if (controller->checkingEnd){
            TFMPCondSignal(controller->read_cond, controller->read_mutex);
            
            pthread_create(&controller->signalThread, nullptr, PlayController::signalPlayFinished, controller);
            pthread_detach(controller->signalThread);
        }else{
            
            if (controller->prepareForSeeking) {
                return false;
            }
            
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
