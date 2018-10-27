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

//debug
#include "TFStateObserver.hpp"
#include "TFMPDebugFuncs.h"

#define TFMPBufferReadLog(fmt,...) printf(fmt,__VA_ARGS__);printf("\n");

using namespace tfmpcore;

static double minExeTime = 0.01; //seconds

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
    
    if (showVideo) {
        pthread_create(&dispalyThread, nullptr, displayLoop, this);
        pthread_detach(dispalyThread);
    }
}

void DisplayController::stopDisplay(){
    shouldDisplay = false;
    printf("shouldDisplay to false\n");
}

void DisplayController::pause(bool flag){
    if (paused == flag) {
        return;
    }
    
    paused = flag;
    videoClock->paused = flag;
    audioClock->paused = flag;
    
    if (!paused) {
        TFMPCondSignal(video_pause_cond, video_pause_mutex)
    }
}

double DisplayController::getPlayTime(){
    if (videoTimeBase.den == 0 || videoTimeBase.num == 0 || lastPts < 0) {
        return invalidPlayTime;
    }
    
    return lastPts * av_q2d(lastIsAudio?audioTimeBase:videoTimeBase);
}

void DisplayController::flush(){
    
    paused = true;
    
    
    bool handleVideo = processingVideo, handleAudio = fillingAudio;
    if (handleVideo) {
        
        shareVideoBuffer->disableIO(true);
        sem_wait(wait_display_sem);
    }
    if (handleAudio) {
        
        shareAudioBuffer->disableIO(true);
        sem_wait(wait_display_sem);
    }
    
    
    remainingAudioBuffers.validSize = 0;
    remainingAudioBuffers.readIndex = 0;
    
    if (handleVideo) {
        shareVideoBuffer->disableIO(false);
    }
    if (handleAudio) {
        shareAudioBuffer->disableIO(false);
    }
    paused = false;
    TFMPCondSignal(video_pause_cond, video_pause_mutex)
}

void DisplayController::freeResources(){
    
    shouldDisplay = false;
    paused = false;
    
    if (processingVideo) {
        sem_wait(wait_display_sem);
    }
    if (fillingAudio) {
        sem_wait(wait_display_sem);
    }
    
    if(audioResampler) audioResampler->freeResources();
    
    free(remainingAudioBuffers.head);
    remainingAudioBuffers.validSize = 0;
    remainingAudioBuffers.readIndex = 0;
    
    displayContext = nullptr;
    shareVideoBuffer = nullptr;
    shareAudioBuffer = nullptr;
    displayMediaType = TFMP_MEDIA_TYPE_ALL_AVIABLE;
}

void *DisplayController::displayLoop(void *context){
    
    DisplayController *displayer = (DisplayController *)context;
    
    TFMPFrame *videoFrame = nullptr;
    
    double time = 0;
    uint64_t lastpts = 0;
    
    
    while (displayer->shouldDisplay) {
        
        videoFrame = nullptr; //reset it
        displayer->processingVideo = true;
        
        if (displayer->paused) {
            
            pthread_mutex_lock(&displayer->video_pause_mutex);
            displayer->processingVideo = false;
            if (displayer->paused) {  //must put condition inside the lock.
                sem_post(displayer->wait_display_sem);
                
                pthread_cond_wait(&displayer->video_pause_cond, &displayer->video_pause_mutex);
            }
            displayer->processingVideo = true;
            
            pthread_mutex_unlock(&displayer->video_pause_mutex);
        }
        
        displayer->shareVideoBuffer->blockGetOut(&videoFrame);
        if (videoFrame == nullptr ) continue;
        if (videoFrame->serial != displayer->serial){
            videoFrame->freeFrameFunc(&videoFrame);
            continue;
        }

        double pts = videoFrame->pts*av_q2d(displayer->videoTimeBase);
        double remainTime = displayer->videoClock->getDelay(pts);
        
        if (remainTime < -minExeTime){
            videoFrame->freeFrameFunc(&videoFrame);
            continue;
        }else if (remainTime > minExeTime) {
            av_usleep(remainTime*1000000);
        }
        TFMPVideoFrameBuffer *displayBuffer = videoFrame->displayBuffer;
        if (displayer->shouldDisplay){
            
            displayer->displayVideoFrame(displayBuffer, displayer->displayContext);
            if(!displayer->paused) {
                if (!displayer->syncClock->isAudioMajor) {
                    displayer->lastPts = videoFrame->pts;
                    displayer->lastIsAudio = false;
                }
                displayer->syncClock->presentVideo(videoFrame->pts, displayer->videoTimeBase);
            }
        }
        
        videoFrame->freeFrameFunc(&videoFrame);
    }
    
    return 0;
}

int DisplayController::fillAudioBuffer(uint8_t **buffersList, int lineCount, int oneLineSize, void *context){
    
    DisplayController *displayer = (DisplayController *)context;
    
    if (!displayer->shouldDisplay){
        return 0;
    }
    
    uint8_t *buffer = buffersList[0];
    if (displayer->paused) {
        printf("display pause audio\n");
        memset(buffer, 0, oneLineSize);
        return 0;
    }
    
    displayer->fillingAudio = true;
    
    TFMPRemainingBuffer *remainingBuffer = &(displayer->remainingAudioBuffers);
    uint32_t unreadSize = remainingBuffer->unreadSize();
    
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
        }
        
        TFMPFrame *audioFrame = nullptr;
        
        bool resample = false;
        uint8_t *dataBuffer = nullptr;
        int linesize = 0, outSamples = 0;
        
        while (needReadSize > 0) {
            
            audioFrame = nullptr;
            displayer->displayingAudio = nullptr;
            
            if (displayer->shareAudioBuffer->isEmpty() || displayer->paused) {
                //fill remain buffer to 0.
                memset(buffer+(oneLineSize - needReadSize), 0, needReadSize);
                break;
            }else{
                displayer->shareAudioBuffer->blockGetOut(&audioFrame);
                displayer->displayingAudio = audioFrame;
            }
            
            //TODO: need more calm way to wait
            if (audioFrame == nullptr || audioFrame->serial != displayer->serial) continue;
            
            
            double remainTime = displayer->syncClock->remainTimeForAudio(audioFrame->pts, displayer->audioTimeBase);
            
            if (remainTime < -minExeTime){
                audioFrame->freeFrameFunc(&audioFrame);
                
                continue;
            }
            
            AVFrame *frame = audioFrame->frame;
            if (displayer->audioResampler->isNeedResample(frame)) {
                if (displayer->audioResampler->reampleAudioFrame(frame, &outSamples, &linesize)) {
                    dataBuffer = displayer->audioResampler->resampledBuffers;
                    resample = true;
                    
//                    TFMPBufferReadLog("resample %d, %d",linesize, outSamples);
                }
                
            }else{
                dataBuffer = frame->extended_data[0];
                linesize = frame->linesize[0];
            }
            
            if (dataBuffer == nullptr) {
                audioFrame->freeFrameFunc(&audioFrame);
                continue;
            }
            
            int filledSize = oneLineSize - needReadSize;
            int destSampleRate = displayer->audioResampler->adoptedAudioDesc.sampleRate;
            int destBytesPerChannel =  displayer->audioResampler->adoptedAudioDesc.bitsPerChannel/8;
            double preBufferDuration = (filledSize/destBytesPerChannel)/destSampleRate;
            
            //update sync clock
            
            if(!displayer->paused) {
                if (displayer->syncClock->isAudioMajor) {
                    displayer->lastPts = frame->pts;
                    displayer->lastIsAudio = true;
                }
                displayer->syncClock->presentAudio(frame->pts, displayer->audioTimeBase, preBufferDuration);
            }
            
            if (needReadSize >= linesize) {
                
                //buffer has be copyed some data what size is oneLineSize - needReadSize.
                memcpy(buffer+(oneLineSize - needReadSize), dataBuffer, linesize);
                needReadSize -= linesize;
                
                audioFrame->freeFrameFunc(&audioFrame);
                
            }else{
                
                //there is a little buffer left.
                uint32_t remainingSize = linesize - needReadSize;
            
                //alloc a larger memory
                if (remainingBuffer->allocSize < remainingSize) {
                    free(remainingBuffer->head);
                    remainingBuffer->head = (uint8_t *)malloc(remainingSize);
                    remainingBuffer->allocSize = remainingSize;
                }
                
                remainingBuffer->readIndex = 0;
                remainingBuffer->validSize = remainingSize;
                
                memcpy(remainingBuffer->head, dataBuffer + needReadSize, remainingSize);
                
                
                memcpy(buffer+(oneLineSize - needReadSize), dataBuffer, needReadSize);
                needReadSize = 0;
                
                audioFrame->freeFrameFunc(&audioFrame);
            }
            
            displayer->displayingAudio = nullptr;
        }
    }
    
    
    if (displayer->paused) {
        displayer->fillingAudio = false;
        sem_post(displayer->wait_display_sem);
    }
    
    
    displayer->fillingAudio = false;
    
    return 0;
}

TFMPFillAudioBufferStruct DisplayController::getFillAudioBufferStruct(){
    return {fillAudioBuffer, this};
}
