//
//  DisplayController.cpp
//  TFMediaPlayer
//
//  Created by shiwei on 17/12/29.
//  Copyright © 2017年 shiwei. All rights reserved.
//

#include "DisplayController.hpp"
#include "TFMPDebugFuncs.h"
extern "C"{
#include <libavutil/time.h>
}

#define TFMPBufferReadLog(fmt,...) printf(fmt,__VA_ARGS__);printf("\n");

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
    
    bool showVideo = displayMediaType & TFMP_MEDIA_TYPE_VIDEO;
    
    if (showVideo) pthread_create(&dispalyThread, nullptr, displayLoop, this);
}

void DisplayController::stopDisplay(){
    shouldDisplay = false;
}

void *DisplayController::displayLoop(void *context){
    
    DisplayController *controller = (DisplayController *)context;
    
    AVFrame *videoFrame = av_frame_alloc(), *audioFrame = av_frame_alloc();
    bool showVideo = controller->displayMediaType & TFMP_MEDIA_TYPE_VIDEO;
//    bool showAudio = controller->displayMediaType & TFMP_MEDIA_TYPE_AUDIO;
    
    while (controller->shouldDisplay) {
        
        printf("got display frame1\n");
        
        if (showVideo) {
            controller->shareVideoBuffer->blockGetOut(&videoFrame);
            printf("got video frame\n");
        }
//        if (showAudio) {
//            controller->shareAudioBuffer->blockGetOut(&audioFrame);
//            printf("got audio frame\n");
//        }
        
        int64_t nextMediaPts = controller->syncClock->nextMediaPts(videoFrame->pts, 0);
        
        if (showVideo) {
            while (videoFrame->pts < nextMediaPts) {
                controller->shareVideoBuffer->getOut(nullptr);
                controller->shareVideoBuffer->back(&videoFrame);
            }
        }
        
//        if (showAudio) {
//            while (audioFrame->pts < nextMediaPts) {
//                controller->shareAudioBuffer->getOut(nullptr);
//                controller->shareAudioBuffer->back(&audioFrame);
//            }
//        }
        
        
        double remainTime = controller->syncClock->remainTime(videoFrame->pts * av_q2d(controller->videoTimeBase), audioFrame->pts * av_q2d(controller->audioTimeBase));
        
        TFMPBufferReadLog("remainTime: %.6f\n",remainTime);
        
        if (remainTime > minExeTime) {
            av_usleep(remainTime*1000000);
        }else if (remainTime < 0){
            continue;
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
    }
    
    return 0;
}

int DisplayController::fillAudioBuffer(uint8_t **buffersList, int lineCount, int oneLineSize, void *context){
    
    TFMPBufferReadLog("\n------start read--------%d,%d",oneLineSize,lineCount);
    
    DisplayController *displayer = (DisplayController *)context;
    
    TFMPRemainingBuffer *remainingBuffer = &(displayer->remainingAudioBuffers);
    uint32_t unreadSize = remainingBuffer->unreadSize();
    
    uint8_t *buffer = buffersList[0];
    
    if (unreadSize >= oneLineSize) {
        
        memcpy(buffer, remainingBuffer->readingPoint(), oneLineSize);
        
        remainingBuffer->readIndex += oneLineSize;
        
        TFMPBufferReadLog("copy remaining[少%d,余%d:总%d]",oneLineSize,remainingBuffer->unreadSize(),remainingBuffer->validSize);
        
    }else{
        
        int needReadSize = oneLineSize;
        if (unreadSize > 0) {
            
            needReadSize -= unreadSize;
            memcpy(buffer, remainingBuffer->readingPoint(), unreadSize);
            
            remainingBuffer->readIndex = 0;
            remainingBuffer->validSize = 0;
            
            TFMPBufferReadLog("copy remaining[少%d,余%d:总%d], need:%d",unreadSize,remainingBuffer->unreadSize(),remainingBuffer->validSize, needReadSize);
            
            unreadSize = 0;
        }
        
        AVFrame *frame = av_frame_alloc();
        
        bool resample = false;
        uint8_t *dataBuffer = nullptr;
        int linesize = 0, outSamples = 0;
        
        while (needReadSize > 0) {
            //TODO: do more thing for planar audio.
            displayer->shareAudioBuffer->blockGetOut(&frame);
            
            TFMPBufferReadLog("new frame %d,%d",frame->linesize[0], frame->nb_samples);
            
            if (displayer->audioResampler->isNeedResample(frame)) {
                if (displayer->audioResampler->reampleAudioFrame(frame, &outSamples, &linesize)) {
                    dataBuffer = displayer->audioResampler->resampledBuffers;
                    resample = true;
                    
                    TFMPBufferReadLog("resample %d, %d",linesize, outSamples);
                }
                
            }else{
                dataBuffer = frame->extended_data[0];
                linesize = frame->linesize[0];
            }
            
            if (dataBuffer == nullptr) {
                av_frame_free(&frame);
                continue;
            }
            
            if (needReadSize >= linesize) {
                
                memcpy(buffer, dataBuffer, linesize);
                needReadSize -= linesize;
                
                TFMPBufferReadLog("copy frame %d",oneLineSize);
                
                av_frame_free(&frame);
                
            }else{
                
                //there is a little buffer left.
                uint32_t remainingSize = linesize - needReadSize;
            
                //alloc a larger memory
                if (remainingBuffer->validSize < remainingSize) {
                    free(remainingBuffer->head);
                    remainingBuffer->head = (uint8_t *)malloc(remainingSize);
                }
                
                remainingBuffer->readIndex = 0;
                remainingBuffer->validSize = remainingSize;
                
                TFMPBufferReadLog("copy last %d, remain[加%d,余%d:总%d], frame:%d",needReadSize,remainingSize,remainingBuffer->unreadSize(),remainingBuffer->validSize, linesize);
                
                memcpy(remainingBuffer->head, dataBuffer + needReadSize, remainingSize);
                
                
                memcpy(buffer, dataBuffer, needReadSize);
                needReadSize = 0;
                
                av_frame_free(&frame);
            }
        }
    }
    return 0;
}

TFMPFillAudioBufferStruct DisplayController::getFillAudioBufferStruct(){
    return {fillAudioBuffer, this};
}




