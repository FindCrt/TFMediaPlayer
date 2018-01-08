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

static unsigned int minExeTime = 0.01; //microsecond

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
    
    AVFrame *videoFrame = av_frame_alloc(), *audioFrame = av_frame_alloc();
    bool showVideo = controller->displayMediaType & TFMP_MEDIA_TYPE_VIDEO;
    bool showAudio = controller->displayMediaType & TFMP_MEDIA_TYPE_AUDIO;
    
    while (controller->shouldDisplay) {
        
        //just check, don't use.
        if (showVideo) {
            controller->shareVideoBuffer->blockGetOut(&videoFrame);
        }
        if (showAudio) {
            controller->shareAudioBuffer->blockGetOut(&audioFrame);
        }
        
        int64_t nextMediaPts = controller->syncClock->nextMediaPts(videoFrame->pts, audioFrame->pts);
        
        if (showVideo) {
            while (videoFrame->pts < nextMediaPts) {
                controller->shareVideoBuffer->getOut(nullptr);
                controller->shareVideoBuffer->back(&videoFrame);
            }
        }
        
        if (showAudio) {
            while (audioFrame->pts < nextMediaPts) {
                controller->shareAudioBuffer->getOut(nullptr);
                controller->shareAudioBuffer->back(&audioFrame);
            }
        }
        
        
        double remainTime = controller->syncClock->remainTime(videoFrame->pts * av_q2d(controller->videoTimeBase), audioFrame->pts * av_q2d(controller->audioTimeBase));
        
        printf("remainTime: %.6f\n",remainTime);
        
        if (remainTime > minExeTime) {
            av_usleep(remainTime*1000000);
        }
        
        TFMPVideoFrameBuffer videoFrameBuf;
        videoFrameBuf.width = videoFrame->width;
        videoFrameBuf.height = videoFrame->height;
        
        for (int i = 0; i<AV_NUM_DATA_POINTERS; i++) {
            
            videoFrameBuf.pixels[i] = videoFrame->data[i];
            videoFrameBuf.linesize[i] = videoFrame->linesize[i];
        }
        
        
        //TODO: unsupport format
        if (videoFrame->format == AV_PIX_FMT_YUV420P) {
            videoFrameBuf.format = TFMP_VIDEO_PIX_FMT_YUV420P;
        }else if (videoFrame->format == AV_PIX_FMT_NV12){
            videoFrameBuf.format = TFMP_VIDEO_PIX_FMT_NV12;
        }else if (videoFrame->format == AV_PIX_FMT_NV21){
            videoFrameBuf.format = TFMP_VIDEO_PIX_FMT_NV21;
        }else if (videoFrame->format == AV_PIX_FMT_RGB32){
            videoFrameBuf.format = TFMP_VIDEO_PIX_FMT_RGB32;
        }
        
        controller->displayVideoFrame(&videoFrameBuf, controller->displayContext);
        
        //TODO: audio
        
        controller->syncClock->correctWithPresent(videoFrame->pts * av_q2d(controller->videoTimeBase), audioFrame->pts * av_q2d(controller->audioTimeBase));
        if(showVideo) av_frame_unref(videoFrame);
        if(showAudio) av_frame_unref(audioFrame);
    }
    
    return 0;
}

<<<<<<< Updated upstream
static int fillAudioBuffer(void *buffer, int size, void *context){
    
    return 0;
}

TFMPFillAudioBufferStruct DisplayController::getFillAudioBufferStruct(){
    return {fillAudioBuffer, this};
}
=======
int DisplayController::fillAudioBuffer(void *buffer, int64_t size, void *context){
    
    return 0;
}
>>>>>>> Stashed changes
