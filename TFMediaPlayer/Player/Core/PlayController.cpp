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
    
    displayer->encounterEndContext = this;
    displayer->encounterEndCallBack = checkEnd;
    
    displayer->newFrameContext = this;
    displayer->newFrameCallBack = seekEnd;
    
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
    
    pthread_create(&readThread, nullptr, readFrame, this);
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
}


void PlayController::pause(bool flag){
    
    paused = flag;
    
    //just change state of displaying, don't change state of reading.
    displayer->pause(flag);
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
        playStoped(this, TFMP_STOP_REASON_USER_STOP);
    }
}

void PlayController::seekTo(double time){
    
    if (time >= duration) {
        time = duration-0.1;
    }
    
    seekPos = time;
    seeking = true;
    
    TFMPDLOG_C("seeking to %.6f\n",seekPos);
}

void PlayController::seekByForward(double interval){
    double currentTime = getCurrentTime();
    
    double seekTime = currentTime + interval;
    seekTo(seekTime);
}

void PlayController::seekEnd(void *context){
    PlayController *controller = (PlayController *)context;
    
    controller->seeking = false;
    if (controller->seekingEndNotify) controller->seekingEndNotify(controller);
}

bool PlayController::checkEnd(void *context){
    PlayController *controller = (PlayController *)context;
    
    bool flag = false;
    if (controller->videoDecoder &&
        controller->videoDecoder->isEmpty()) {
        flag = true;
    }
    
    if (controller->audioDecoder &&
        controller->audioDecoder->isEmpty()) {
        flag = true;
    }
    
    if (flag) {
        controller->playStoped(controller, TFMP_STOP_REASON_END_OF_FILE);
    }
    
    return flag;
}



void PlayController::resetStatus(){
    desiredDisplayMediaType = TFMP_MEDIA_TYPE_ALL_AVIABLE;
    realDisplayMediaType = TFMP_MEDIA_TYPE_NONE;
    
    videoStrem = -1;
    audioStream = -1;
    subTitleStream = -1;
    
    abortRequest = false;
    seeking = false;
    prapareOK = false;
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
    if (seeking || playTime < 0) {
        playTime = seekPos;
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

void * PlayController::readFrame(void *context){
    
    PlayController *controller = (PlayController *)context;
    
    AVPacket *packet = nullptr;
    
    while (!controller->abortRequest) {
        
        if (controller->seeking) {
            controller->pause(true);
            int retval = avformat_seek_file(controller->fmtCtx, -1, INT64_MIN, controller->seekPos*AV_TIME_BASE, INT64_MAX, 0);
            if (retval == 0) {
                if (controller->audioDecoder) {
                    controller->audioDecoder->serial++;
                }
                if (controller->videoDecoder) {
                    controller->videoDecoder->serial++;
                }
                controller->displayer->serial++;
                
                if (controller->accurateSeek){
                    controller->displayer->filterTime = controller->seekPos;
                }else{
                    controller->displayer->filterTime = 0;
                }
                //seek之后，从新的地方开始读取，不确定是否结束；如果还是结束，等再次遇到AVERROR_EOF错误还会重新标记，这里先重置
                controller->displayer->checkingEnd = false;
            }
            controller->seeking = false;
//            controller->seekingEndNotify(controller);
            controller->pause(false);
        }
        
        packet = av_packet_alloc();
        int retval = av_read_frame(controller->fmtCtx, packet);
        
        if(retval < 0){
            if (retval == AVERROR_EOF) {
                controller->displayer->checkingEnd = true;
                
                av_packet_free(&packet);
                av_usleep(100); //等下之后的处理，可能还会继续seek
                TFMPDLOG_C("end of file\n");
                continue;
            }else{
                av_packet_free(&packet);
                continue;
            }
        }
        
        if ((controller->realDisplayMediaType & TFMP_MEDIA_TYPE_VIDEO) &&
            packet->stream_index == controller->videoStrem) {
            
            controller->videoDecoder->insertPacket(packet);
           
        }else if ((controller->realDisplayMediaType & TFMP_MEDIA_TYPE_AUDIO) &&
                  packet->stream_index == controller->audioStream){
            
            controller->audioDecoder->insertPacket(packet);
            
        }else if ((controller->realDisplayMediaType & TFMP_MEDIA_TYPE_SUBTITLE) &&
                  packet->stream_index == controller->subTitleStream){
            controller->subtitleDecoder->insertPacket(packet);
        }
    }
    
    return 0;
}
