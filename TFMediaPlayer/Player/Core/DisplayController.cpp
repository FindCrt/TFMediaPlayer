//
//  DisplayController.cpp
//  TFMediaPlayer
//
//  Created by shiwei on 17/12/29.
//  Copyright © 2017年 shiwei. All rights reserved.
//

#include "DisplayController.hpp"
extern "C"{
#include <libavutil/time.h>
}

using namespace tfmpcore;

static unsigned int minExeTime = 10000; //microsecond

void DisplayController::startDisplay(){
    
    if (shareVideoBuffer == nullptr && shareAudioBuffer == nullptr) {
        return;
    }
    
    if ((shareVideoBuffer != nullptr && displayVideoFrame == nullptr) ||
        (shareAudioBuffer != nullptr && displayVideoFrame == nullptr))
    {
        return;
    }
    
    shouldDisplay = true;
    
    pthread_create(&dispalyThread, nullptr, displayLoop, this);
    
}

void DisplayController::stopDisplay(){
    shouldDisplay = false;
}

void *DisplayController::displayLoop(void *context){
    
    DisplayController *controller = (DisplayController *)context;
    
    AVFrame videoFrame, audioFrame;
    
    while (controller->shouldDisplay) {
        
        //just check, don't use.
        controller->shareVideoBuffer->back(&videoFrame);
        controller->shareAudioBuffer->back(&audioFrame);
        
        int64_t nextMediaPts = controller->syncClock->nextMediaPts(videoFrame.pts, audioFrame.pts);
        
        while (videoFrame.pts < nextMediaPts) {
            controller->shareVideoBuffer->getOut(nullptr);
            controller->shareVideoBuffer->back(&videoFrame);
        }
        
        while (audioFrame.pts < nextMediaPts) {
            controller->shareAudioBuffer->getOut(nullptr);
            controller->shareAudioBuffer->back(&audioFrame);
        }
        
        int64_t remainTime = controller->syncClock->remainTime(videoFrame.pts, audioFrame.pts);
        
        if (remainTime > minExeTime) {
            av_usleep((unsigned int)remainTime);
        }
        
        TFMPVideoFrameBuffer videoFrameBuf;
        videoFrameBuf.width = videoFrame.width;
        videoFrame.height = videoFrame.height;
        
        //TODO: unsupport format
        if (videoFrame.format == AV_PIX_FMT_YUV420P) {
            videoFrameBuf.format = TFMP_VIDEO_PIX_FMT_YUV420P;
        }else if (videoFrame.format == AV_PIX_FMT_NV12){
            videoFrameBuf.format = TFMP_VIDEO_PIX_FMT_NV12;
        }else if (videoFrame.format == AV_PIX_FMT_NV21){
            videoFrameBuf.format = TFMP_VIDEO_PIX_FMT_NV21;
        }else if (videoFrame.format == AV_PIX_FMT_RGB32){
            videoFrameBuf.format = TFMP_VIDEO_PIX_FMT_RGB32;
        }
        
        controller->displayVideoFrame(&videoFrameBuf, controller->displayContext);
        
        //TODO: audio
        
        controller->shareAudioBuffer->getOut(nullptr);
        controller->shareAudioBuffer->getOut(nullptr);
        
        controller->syncClock->correctWithPresent(videoFrame.pts, audioFrame.pts);
    }
    
    return 0;
}
