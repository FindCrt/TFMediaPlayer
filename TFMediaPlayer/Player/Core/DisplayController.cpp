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

#define TFMPBufferReadLog(fmt,...) //printf(fmt,__VA_ARGS__);printf("\n");

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
    
    AVFrame *videoFrame = nullptr;
    
    while (controller->shouldDisplay) {
        
        controller->shareVideoBuffer->blockGetOut(&videoFrame);
        
        int64_t nextMediaPts = controller->syncClock->nextMediaPts(videoFrame->pts, 0);
        
        while (videoFrame->pts < nextMediaPts) {
            controller->shareVideoBuffer->getOut(nullptr);
            controller->shareVideoBuffer->back(&videoFrame);
        }
        
        double remainTime = controller->syncClock->remainTime(videoFrame->pts * av_q2d(controller->videoTimeBase), 0);
        
        TFMPBufferReadLog("remainTime: %.6f\n",remainTime);
        
        if (remainTime > minExeTime) {
            av_usleep(remainTime*1000000);
        }else if (remainTime < 0){
            continue;
        }
        
        TFMPVideoFrameBuffer *interimBuffer = new TFMPVideoFrameBuffer();
        interimBuffer->width = videoFrame->width;
        interimBuffer->height = videoFrame->height;
        
        for (int i = 0; i<AV_NUM_DATA_POINTERS; i++) {
            
            interimBuffer->pixels[i] = videoFrame->data[i]+videoFrame->linesize[i];
            interimBuffer->linesize[i] = videoFrame->linesize[i];
        }
        
        //TODO: unsupport format
        if (videoFrame->format == AV_PIX_FMT_YUV420P) {
            interimBuffer->format = TFMP_VIDEO_PIX_FMT_YUV420P;
        }else if (videoFrame->format == AV_PIX_FMT_NV12){
            interimBuffer->format = TFMP_VIDEO_PIX_FMT_NV12;
        }else if (videoFrame->format == AV_PIX_FMT_NV21){
            interimBuffer->format = TFMP_VIDEO_PIX_FMT_NV21;
        }else if (videoFrame->format == AV_PIX_FMT_RGB32){
            interimBuffer->format = TFMP_VIDEO_PIX_FMT_RGB32;
        }
        
        controller->displayVideoFrame(interimBuffer, controller->displayContext);
        
        controller->syncClock->correctWithPresent(videoFrame->pts * av_q2d(controller->videoTimeBase), 0);
        
        av_frame_free(&videoFrame);
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
        
        AVFrame *frame = nullptr;
        
        bool resample = false;
        uint8_t *dataBuffer = nullptr;
        int linesize = 0, outSamples = 0;
        
        while (needReadSize > 0) {
            //TODO: do more thing for planar audio.
            frame = av_frame_alloc();
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
                
                //buffer has be copyed some data what size is oneLineSize - needReadSize.
                memcpy(buffer+(oneLineSize - needReadSize), dataBuffer, linesize);
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
                
                
                memcpy(buffer+(oneLineSize - needReadSize), dataBuffer, needReadSize);
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




