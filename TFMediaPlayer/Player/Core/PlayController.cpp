//
//  PlayController.cpp
//  TFMediaPlayer
//
//  Created by shiwei on 17/12/25.
//  Copyright © 2017年 shiwei. All rights reserved.
//

#include "PlayController.hpp"
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

#include "DebugFuncs.h"

using namespace tfmpcore;

bool PlayController::connectAndOpenMedia(std::string mediaPath){
    av_register_all();
    
    AVFormatContext *fmtCtx = avformat_alloc_context();
    int retval= avformat_open_input(&fmtCtx, mediaPath.c_str(), NULL, NULL);
    TFCheckRetvalAndGotoFail("avformat_open_input");
    
Fail:
    return false;
}

void PlayController::play(){
    
}

void PlayController::pause(){
    
}

void PlayController::stop(){
    
}
