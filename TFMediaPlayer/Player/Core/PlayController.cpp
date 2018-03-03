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
            videoStrem = i;
        }else if (type == AVMEDIA_TYPE_AUDIO){
            audioDecoder = new Decoder(fmtCtx, i, type);
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
    
    
    
    if (audioStream) {
        duration = fmtCtx->streams[audioStream]->duration* av_q2d(fmtCtx->streams[audioStream]->time_base);
    }else if (videoStrem){
        duration = fmtCtx->streams[videoStrem]->duration* av_q2d(fmtCtx->streams[videoStrem]->time_base);
    }
    
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
}

void PlayController::pause(){
    
    shouldRead = false;
    
    displayer->stopDisplay();
    
    if (videoDecoder) {
        videoDecoder->stopDecode();
    }
    if (audioDecoder) {
        audioDecoder->stopDecode();
    }
    if (subtitleDecoder) {
        subtitleDecoder->stopDecode();
    }
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
    
    if (time > duration) {
        time = duration-0.1;
    }
    
    if (videoDecoder) {
        videoDecoder->flush();
    }
    if (audioDecoder) {
        audioDecoder->flush();
    }
    if (subtitleDecoder) {
        subtitleDecoder->flush();
    }
    
    displayer->syncClock->reset();
    
    TFMPDLOG_C("flush all end!\n");
    
    if (videoStrem) {
        av_seek_frame(fmtCtx, videoStrem, time/av_q2d(fmtCtx->streams[videoStrem]->time_base), AVSEEK_FLAG_BACKWARD);
    }
    if (audioStream) {
        av_seek_frame(fmtCtx, audioStream, time/av_q2d(fmtCtx->streams[audioStream]->time_base), AVSEEK_FLAG_BACKWARD);
    }
    
    TFMPDLOG_C("seek end! %.3f\n",time);
}

void PlayController::seekByForward(double interval){
    double currentTime = displayer->getCurrentPlayTime();
    
    double seekTime = currentTime + interval;
    seekTo(seekTime);
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

static int inframeCount = 0;
void * PlayController::readFrame(void *context){
    
    PlayController *controller = (PlayController *)context;
    
    AVPacket *packet = av_packet_alloc();
    
    bool endFile = false;
    
    while (controller->shouldRead) {
        
        int retval = av_read_frame(controller->fmtCtx, packet);
        
        TFMPDLOG_C("read frame: %d\n",inframeCount++);
        
        if(retval < 0){
            if (retval == AVERROR_EOF) {
                endFile = true;
                break;
            }else{
                continue;
            }
        }
        
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
    
    if (endFile) controller->startCheckPlayFinish();
    
    return 0;
}

/** file has reach the end, if the data in packet buffer and frame buffer are used, all resources is showed then now it's need to stop.*/
void PlayController::startCheckPlayFinish(){
    
    Decoder *checkDecoder = nullptr;
    
    if (realDisplayMediaType & TFMP_MEDIA_TYPE_AUDIO) {
        checkDecoder = audioDecoder;
    }else if(realDisplayMediaType & TFMP_MEDIA_TYPE_VIDEO){
        checkDecoder = videoDecoder;
    }
    
    if (!checkDecoder) {
        if (playStoped) {
            playStoped(this, 0);
        }
    }
    
    while (!checkDecoder->bufferIsEmpty()) {
        av_usleep(10000);
    }
    
    if (playStoped) {
        playStoped(this, 0);
    }
}
