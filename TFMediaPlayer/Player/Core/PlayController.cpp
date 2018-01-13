//
//  PlayController.cpp
//  TFMediaPlayer
//
//  Created by shiwei on 17/12/25.
//  Copyright © 2017年 shiwei. All rights reserved.
//

#include "PlayController.hpp"
#include "TFMPDebugFuncs.h"
#include "ThreadConvience.h"
#include "TFMPUtilities.h"

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
    
    if (audioStream < 0 && isAudioMajor) isAudioMajor = false;
    if (videoStrem <0 && !isAudioMajor) isAudioMajor = true;
    displayer->syncClock = new SyncClock(isAudioMajor);

    prapareOK = true;
    
    int realType = 0;
    if ((desiredDisplayMediaType & TFMP_MEDIA_TYPE_VIDEO) && (videoStrem >= 0)) {
        realType |= TFMP_MEDIA_TYPE_VIDEO;
    }
    if ((desiredDisplayMediaType & TFMP_MEDIA_TYPE_AUDIO) && (videoStrem >= 0)) {
        realType |= TFMP_MEDIA_TYPE_AUDIO;
    }
    if ((desiredDisplayMediaType & TFMP_MEDIA_TYPE_SUBTITLE) && (videoStrem >= 0)) {
        realType |= TFMP_MEDIA_TYPE_SUBTITLE;
    }
    displayer->displayMediaType = (TFMPMediaType)realType;
    
    if (connectCompleted != nullptr) {
        connectCompleted(this);
    }
    
    return true;
}

void PlayController::play(){
    if (!prapareOK) {
        return;
    }
    
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
    
}

void PlayController::stop(){
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
}

/***** properties *****/

void PlayController::setDesiredDisplayMediaType(TFMPMediaType desiredDisplayMediaType){
    this->desiredDisplayMediaType = desiredDisplayMediaType;
    if (prapareOK) {
        int realType = 0;
        if ((desiredDisplayMediaType & TFMP_MEDIA_TYPE_VIDEO) && (videoStrem >= 0)) {
            realType |= TFMP_MEDIA_TYPE_VIDEO;
        }
        if ((desiredDisplayMediaType & TFMP_MEDIA_TYPE_AUDIO) && (videoStrem >= 0)) {
            realType |= TFMP_MEDIA_TYPE_AUDIO;
        }
        if ((desiredDisplayMediaType & TFMP_MEDIA_TYPE_SUBTITLE) && (videoStrem >= 0)) {
            realType |= TFMP_MEDIA_TYPE_SUBTITLE;
        }
        
        displayer->displayMediaType = (TFMPMediaType)realType;
    }
}

TFMPFillAudioBufferStruct PlayController::getFillAudioBufferStruct(){
    return displayer->getFillAudioBufferStruct();
}
DisplayController *PlayController::getDisplayer(){
    return displayer;
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
    audioResampler->setAdoptedAudioDesc(adoptedAudioDesc);
    displayer->setAudioResampler(audioResampler);
//    displayer->setAdoptedAudioDesc(adoptedAudioDesc);
}

void PlayController::startReadingFrames(){
    pthread_create(&readThread, nullptr, readFrame, this);
}

void * PlayController::readFrame(void *context){
    
    PlayController *controller = (PlayController *)context;
    
    AVPacket *packet = av_packet_alloc();
    
    while (av_read_frame(controller->fmtCtx, packet) == 0) {
        
        if (packet->stream_index == controller->videoStrem) {
            
            printf(">>>>>>>video packet in\n");
            controller->videoDecoder->decodePacket(packet);
            
        }else if (packet->stream_index == controller->audioStream){
            
            controller->audioDecoder->decodePacket(packet);
            
        }else if (packet->stream_index == controller->subTitleStream){
            
            controller->subtitleDecoder->decodePacket(packet);
        }
    }
    
    return 0;
}

