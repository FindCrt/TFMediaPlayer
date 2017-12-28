//
//  PlayController.cpp
//  TFMediaPlayer
//
//  Created by shiwei on 17/12/25.
//  Copyright © 2017年 shiwei. All rights reserved.
//

#include "PlayController.hpp"
#include "DebugFuncs.h"
#include "ThreadConvience.h"

using namespace tfmpcore;
//
//namespace tfmpcore {
//    void * readFrame(void *context);
//}

bool PlayController::connectAndOpenMedia(std::string mediaPath){
    
    this->mediaPath = mediaPath;
    
    av_register_all();
    
    fmtCtx = avformat_alloc_context();
    int retval = avformat_open_input(&fmtCtx, mediaPath.c_str(), NULL, NULL);
    TFCheckRetvalAndGotoFail("avformat_open_input");
    
    //configure options to get faster
    retval = avformat_find_stream_info(fmtCtx, NULL);
    TFCheckRetvalAndGotoFail("avformat_find_stream_info");
    
    for (int i = 0; i<fmtCtx->nb_streams; i++) {
        
        AVMediaType type = fmtCtx->streams[i]->codecpar->codec_type;
        if (type == AVMEDIA_TYPE_VIDEO) {
            videoDecoder = new Decoder(fmtCtx, i, type);
        }else if (type == AVMEDIA_TYPE_AUDIO){
            audioDecoder = new Decoder(fmtCtx, i, type);
        }else if (type == AVMEDIA_TYPE_SUBTITLE){
            subtitleDecoder = new Decoder(fmtCtx, i, type);
        }
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
    
    prapareOK = true;
    return true;
    
Fail:
    return false;
}

void PlayController::play(){
    if (!prapareOK) {
        return;
    }
    
    startReadingFrames();
}

void PlayController::pause(){
    
}

void PlayController::stop(){
    
}



/***** private ******/

void PlayController::startReadingFrames(){
    pthread_create(&readThread, nullptr, readFrame, this);
}

void * PlayController::readFrame(void *context){
    
    PlayController *controller = (PlayController *)context;
    
    AVPacket packet;
    
    while (av_read_frame(controller->fmtCtx, &packet) > 0) {
        
        if (packet.stream_index == controller->videoStrem) {
            
            controller->videoDecoder->decodePacket(&packet);
            
        }else if (packet.stream_index == controller->audioStream){
            
            controller->audioDecoder->decodePacket(&packet);
            
        }else if (packet.stream_index == controller->subTitleStream){
            
            controller->subtitleDecoder->decodePacket(&packet);
        }
    }
    
    return 0;
}

