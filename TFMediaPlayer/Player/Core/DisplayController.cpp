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

void DisplayController::pause(bool flag){
    if (paused == flag) {
        return;
    }
    TFMPDLOG_C("DisplayController pause: %s\n",flag?"true":"false");
    paused = flag;
    if (paused) {
        syncClock->reset();
    }else{
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
    TFMPDLOG_C("DisplayController::flush\n");
    
    bool handleVideo = processingVideo, handleAudio = fillingAudio;
    if (handleVideo) {
        TFMPDLOG_C("sem wait video\n");
        shareVideoBuffer->disableIO(true);
        sem_wait(wait_display_sem);
    }
    if (handleAudio) {
        TFMPDLOG_C("sem wait audio\n");
        shareAudioBuffer->disableIO(true);
        sem_wait(wait_display_sem);
    }
    TFMPDLOG_C("DisplayController::flush wait end\n");
    
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
    
    AVFrame *videoFrame = nullptr;
    
    myStateObserver.mark("video display", 1);
    while (displayer->shouldDisplay) {
        
        videoFrame = nullptr; //reset it
        displayer->processingVideo = true;
        
        if (displayer->paused) {
            
            pthread_mutex_lock(&displayer->video_pause_mutex);
            displayer->processingVideo = false;
            if (displayer->paused) {  //must put condition inside the lock.
                sem_post(displayer->wait_display_sem);
                myStateObserver.mark("video display", 2);
                pthread_cond_wait(&displayer->video_pause_cond, &displayer->video_pause_mutex);
            }
            displayer->processingVideo = true;
            myStateObserver.mark("video display", 20);
            pthread_mutex_unlock(&displayer->video_pause_mutex);
        }
        
        myStateObserver.mark("video display", 3);
        
        displayer->shareVideoBuffer->blockGetOut(&videoFrame);
        if (videoFrame == nullptr) continue;
        myStateObserver.mark("video display", 4);
        
        double remainTime = displayer->syncClock->remainTimeForVideo(videoFrame->pts, displayer->videoTimeBase);
        myStateObserver.labelMark("video remain", to_string(remainTime)+" pts: "+to_string(videoFrame->pts*av_q2d(displayer->videoTimeBase)));
        
        if (remainTime < -minExeTime){
            av_frame_free(&videoFrame);
            continue;
        }else if (remainTime > minExeTime) {
            TFMPDLOG_C("video sleep: %.3f\n",remainTime);
            av_usleep(remainTime*1000000);
        }
        
        myStateObserver.mark("video display", 5);
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
        
        
        myStateObserver.mark("video display", 6);
        if (displayer->shouldDisplay){
            myStateObserver.mark("video display", 7);
            displayer->displayVideoFrame(interimBuffer, displayer->displayContext);

            if (!displayer->syncClock->isAudioMajor) {
                
                displayer->lastPts = videoFrame->pts;
                displayer->lastIsAudio = false;
                
            }
            myStateObserver.mark("video display", 8);
            if(!displayer->paused) displayer->syncClock->presentVideo(videoFrame->pts, displayer->videoTimeBase);
        }
        
        av_frame_free(&videoFrame);
    }
    
    return 0;
}

int DisplayController::fillAudioBuffer(uint8_t **buffersList, int lineCount, int oneLineSize, void *context){
    myStateObserver.timeMark("fillAudioBuffer");
    DisplayController *displayer = (DisplayController *)context;
    myStateObserver.mark("display audio", 1);
    if (!displayer->shouldDisplay){
        return 0;
    }
    myStateObserver.mark("display audio", 2);
    uint8_t *buffer = buffersList[0];
    if (displayer->paused) {
        printf("display pause audio\n");
        memset(buffer, 0, oneLineSize);
        return 0;
    }
    myStateObserver.mark("display audio", 3);
    displayer->fillingAudio = true;
    
    TFMPRemainingBuffer *remainingBuffer = &(displayer->remainingAudioBuffers);
    uint32_t unreadSize = remainingBuffer->unreadSize();
    
    if (unreadSize >= oneLineSize) {
        memcpy(buffer, remainingBuffer->readingPoint(), oneLineSize);
        
        remainingBuffer->readIndex += oneLineSize;
        myStateObserver.mark("display audio", 4);
        
    }else{
        int needReadSize = oneLineSize;
        if (unreadSize > 0) {
            needReadSize -= unreadSize;
            memcpy(buffer, remainingBuffer->readingPoint(), unreadSize);
            
            remainingBuffer->readIndex = 0;
            remainingBuffer->validSize = 0;
        }
        
        AVFrame *frame = nullptr;
        
        bool resample = false;
        uint8_t *dataBuffer = nullptr;
        int linesize = 0, outSamples = 0;
        myStateObserver.mark("display audio", 5);
        while (needReadSize > 0) {
            
            //TODO: do more thing for planar audio.
            frame = nullptr;
            displayer->displayingAudio = nullptr;
            
            if (displayer->shareAudioBuffer->isEmpty() || displayer->paused) {
                //fill remain buffer to 0.
                memset(buffer+(oneLineSize - needReadSize), 0, needReadSize);
                break;
            }else{
                myStateObserver.mark("display audio", 6);
                displayer->shareAudioBuffer->blockGetOut(&frame);
                
                displayer->displayingAudio = frame;
            }
            
            //TODO: need more calm way to wait
            if (frame == nullptr) continue;
            myStateObserver.mark("display audio", 7);
            double remainTime = displayer->syncClock->remainTimeForAudio(frame->pts, displayer->audioTimeBase);
            myStateObserver.labelMark("audio remain", to_string(remainTime)+" pts "+to_string(frame->pts*av_q2d(displayer->audioTimeBase)));
            if (remainTime < -minExeTime){
                av_frame_free(&frame);
                TFMPDLOG_C("discard audio frame\n");
                continue;
            }
            myStateObserver.mark("display audio", 8);
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
            myStateObserver.mark("display audio", 9);
            if (dataBuffer == nullptr) {
                av_frame_free(&frame);
                continue;
            }
            
            int filledSize = oneLineSize - needReadSize;
            int destSampleRate = displayer->audioResampler->adoptedAudioDesc.sampleRate;
            int destBytesPerChannel =  displayer->audioResampler->adoptedAudioDesc.bitsPerChannel/8;
            double preBufferDuration = (filledSize/destBytesPerChannel)/destSampleRate;
            
            //update sync clock
            if (displayer->syncClock->isAudioMajor) {
                displayer->lastPts = frame->pts;
                displayer->lastIsAudio = true;
                
            }
            myStateObserver.mark("display audio", 10);
            if(!displayer->paused) displayer->syncClock->presentAudio(frame->pts, displayer->audioTimeBase, preBufferDuration);
            
            if (needReadSize >= linesize) {
                
                //buffer has be copyed some data what size is oneLineSize - needReadSize.
                memcpy(buffer+(oneLineSize - needReadSize), dataBuffer, linesize);
                needReadSize -= linesize;
                
                av_frame_free(&frame);
                
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
                
                av_frame_free(&frame);
            }
            
            displayer->displayingAudio = nullptr;
        }
    }
    
    myStateObserver.mark("display audio", 11);
    if (displayer->paused) {
        displayer->fillingAudio = false;
        sem_post(displayer->wait_display_sem);
        TFMPDLOG_C("sem post audio\n");
    }
    
    myStateObserver.mark("display audio", 12);
    displayer->fillingAudio = false;
    
    return 0;
}

TFMPFillAudioBufferStruct DisplayController::getFillAudioBufferStruct(){
    return {fillAudioBuffer, this};
}




