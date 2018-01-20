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

static unsigned int minExeTime = 0.01; //seconds

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
    
    TFMPDLOG_C("shouldDisplay to true\n");
    
    bool showVideo = displayMediaType & TFMP_MEDIA_TYPE_VIDEO;
    
    if (showVideo) {
        pthread_create(&dispalyThread, nullptr, displayLoop, this);
        pthread_detach(dispalyThread);
    }
}

void DisplayController::stopDisplay(){
    shouldDisplay = false;
    printf("shouldDisplay to false\n");
}

void DisplayController::freeResource(){
    
    if (shouldDisplay){
        TFMPDLOG_C("free DisplayController resource before stop display\n");
        shouldDisplay = false;
    }
    
    while (isDispalyingVideo || isFillingAudio) {
        TFMPDLOG_C("waiting video or audio displaying loop end\n");
    }
    
    free(remainingAudioBuffers.head);
    remainingAudioBuffers.validSize = 0;
    remainingAudioBuffers.readIndex = 0;
    
    audioResampler->freeResources();
    free(audioResampler);
    
    free(syncClock);
    
    displayContext = nullptr;
    shareVideoBuffer = nullptr;
    shareAudioBuffer = nullptr;
    displayMediaType = TFMP_MEDIA_TYPE_ALL_AVIABLE;
}

void *DisplayController::displayLoop(void *context){
    
    DisplayController *controller = (DisplayController *)context;
    
    AVFrame *videoFrame = nullptr;
    
    while (controller->shouldDisplay) {
        
        controller->isDispalyingVideo = true;
        
        controller->shareVideoBuffer->blockGetOut(&videoFrame);
        
        double remainTime = controller->syncClock->remainTimeForVideo(videoFrame->pts, controller->videoTimeBase);
        
        if (remainTime > minExeTime) {
            av_usleep(remainTime*1000000);
        }else if (remainTime < -remainTime){
            continue;
        }
        
        TFMPDLOG_C("remainTime: %.6f\n",remainTime);
        
        TFMPVideoFrameBuffer *interimBuffer = new TFMPVideoFrameBuffer();
        interimBuffer->width = videoFrame->width;
        //TODO: when should i cut one line of data to avoid the green data-empty zone in bottom?
        interimBuffer->height = videoFrame->height-1;
        
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
        
        if (controller->shouldDisplay){
            controller->displayVideoFrame(interimBuffer, controller->displayContext);
            controller->syncClock->presentVideo(videoFrame->pts, controller->videoTimeBase);
        }
        
        av_frame_free(&videoFrame);
        
        controller->isDispalyingVideo = false;
    }
    
    return 0;
}

int DisplayController::fillAudioBuffer(uint8_t **buffersList, int lineCount, int oneLineSize, void *context){
    
    DisplayController *displayer = (DisplayController *)context;
    
    TFMPDLOG_C("\n\nstart audio fill 1: %d\n",displayer->shouldDisplay);
    if (!displayer->shouldDisplay){
        return 0;
    }
    
    TFMPDLOG_C("start audio fill 2: %d\n",displayer->shouldDisplay);
    
    displayer->isFillingAudio = true;
    
    TFMPRemainingBuffer *remainingBuffer = &(displayer->remainingAudioBuffers);
    uint32_t unreadSize = remainingBuffer->unreadSize();
    
    uint8_t *buffer = buffersList[0];
    
    if (unreadSize >= oneLineSize) {
        
        memcpy(buffer, remainingBuffer->readingPoint(), oneLineSize);
        
        remainingBuffer->readIndex += oneLineSize;
        
    }else{
        
        int needReadSize = oneLineSize;
        if (unreadSize > 0) {
            
            needReadSize -= unreadSize;
            memcpy(buffer, remainingBuffer->readingPoint(), unreadSize);
            
            remainingBuffer->readIndex = 0;
            remainingBuffer->validSize = 0;
            
            unreadSize = 0;
        }
        
        AVFrame *frame = nullptr;
        
        bool resample = false;
        uint8_t *dataBuffer = nullptr;
        int linesize = 0, outSamples = 0;
        
        while (needReadSize > 0) {
            //TODO: do more thing for planar audio.
            frame = av_frame_alloc();
            TFMPDLOG_C("blockGetOut audio_frame\n");
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
            
            //update sync clock
            int filledSize = oneLineSize - needReadSize;
            int destSampleRate = displayer->audioResampler->adoptedAudioDesc.sampleRate;
            int destBytesPerChannel =  displayer->audioResampler->adoptedAudioDesc.bitsPerChannel/8;
            double preBufferDuration = (filledSize/destBytesPerChannel)/destSampleRate;
            displayer->syncClock->presentAudio(frame->pts, displayer->audioTimeBase, preBufferDuration);
            
            
            if (needReadSize >= linesize) {
                
                //buffer has be copyed some data what size is oneLineSize - needReadSize.
                memcpy(buffer+(oneLineSize - needReadSize), dataBuffer, linesize);
                needReadSize -= linesize;
                
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
                
                memcpy(remainingBuffer->head, dataBuffer + needReadSize, remainingSize);
                
                
                memcpy(buffer+(oneLineSize - needReadSize), dataBuffer, needReadSize);
                needReadSize = 0;
                
                av_frame_free(&frame);
            }
        }
    }
    
    displayer->isFillingAudio = false;
    
    return 0;
}

TFMPFillAudioBufferStruct DisplayController::getFillAudioBufferStruct(){
    return {fillAudioBuffer, this};
}




