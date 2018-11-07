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
static double maxRemainTime = 5;

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
    }
    
#if DEBUG
    audioClock->name = "audioClock";
    videoClock->name = "videoClock";
#endif
}

void DisplayController::stopDisplay(){
    shouldDisplay = false;
    paused = false;
    printf("shouldDisplay to false\n");
    
    TFMPCondSignal(video_pause_cond, video_pause_mutex)
    
    pthread_join(dispalyThread, nullptr);
    
    audioResampler.freeResources();
    
    free(remainingAudioBuffers.head);
    remainingAudioBuffers.validSize = 0;
    remainingAudioBuffers.readIndex = 0;
    
    displayContext = nullptr;
    shareVideoBuffer = nullptr;
    shareAudioBuffer = nullptr;
    displayMediaType = TFMP_MEDIA_TYPE_ALL_AVIABLE;
    
    delete videoClock;
    delete audioClock;
}

void DisplayController::pause(bool flag){
    if (paused == flag) {
        return;
    }
    
    paused = flag;
    
    if (!paused) {
        videoClock->updateDiff();
        audioClock->updateDiff();
        TFMPCondSignal(video_pause_cond, video_pause_mutex)
    }
    
    videoClock->paused = flag;
    audioClock->paused = flag;
}

double DisplayController::getPlayTime(){
    //serial不同，说明同步钟还是seek之间的数据，它里面的时间不可用
    if (getMajorClock()->serial != serial) {
        return -1;
    }
    return getMajorClock()->getTime();
}

void *DisplayController::displayLoop(void *context){
    
    DisplayController *displayer = (DisplayController *)context;
    
    TFMPFrame *videoFrame = nullptr;
    SyncClock *majorClock = displayer->getMajorClock();
    
    while (displayer->shouldDisplay) {
        
        videoFrame = nullptr; //reset it
        
        if (displayer->paused) {
            
            pthread_mutex_lock(&displayer->video_pause_mutex);
            if (displayer->paused) {  //must put condition inside the lock.
                pthread_cond_wait(&displayer->video_pause_cond, &displayer->video_pause_mutex);
            }
            
            pthread_mutex_unlock(&displayer->video_pause_mutex);
        }
        
        if (displayer->checkingEnd && displayer->shareVideoBuffer->isEmpty()) {
            
            //回调给PlayController，返回true表明确实结束了，退出播放
            if (displayer->encounterEndCallBack(displayer->encounterEndContext)) {
                break;
            }
        }
        
        displayer->shareVideoBuffer->blockGetOut(&videoFrame);
        
        if (videoFrame == nullptr ) continue;
        if (videoFrame->serial != displayer->serial){
            videoFrame->freeFrameFunc(&videoFrame);
            continue;
        }
        
        double pts = videoFrame->pts*av_q2d(displayer->videoTimeBase);
        
        //使用了时间筛选，早于这个时间的都去掉
        if (displayer->filterTime && pts<displayer->filterTime) {
            videoFrame->freeFrameFunc(&videoFrame);
            continue;
        }
        
        double remainTime = 0;
        if (videoFrame->serial == majorClock->serial) {
//            remainTime = majorClock->getRemainTime(pts);
        }
        
        if (videoFrame->serial != displayer->videoClock->serial) {
            //新旧frame的临界点,表明seek正式结束
            displayer->newFrameCallBack(displayer->newFrameContext);
        }
        
        if (remainTime>maxRemainTime) {
            //计算失误或者某些特殊情况导致视频进度远快于主钟进度，不能直接卡死视频，延迟调为平均时长的两倍，稍微的减慢速度，多个帧逐渐的拉平差距
            remainTime = displayer->averageVideoDu*2;
        }
        
        if (remainTime < -minExeTime){
            videoFrame->freeFrameFunc(&videoFrame);
            continue;
        }else if (remainTime > minExeTime) {
            av_usleep(remainTime*1000000);
        }
        
        TFMPVideoFrameBuffer *displayBuffer = videoFrame->displayBuffer;
        displayer->displayVideoFrame(displayBuffer, displayer->displayContext);
        
        //传入videoFrame的serial而不是displayer的是关键一步，保持clock和上次显示的frame的serial一致，和上面`getRemainTime`时的判断条件一致
        displayer->videoClock->updateTime(pts, videoFrame->serial);
        
        videoFrame->freeFrameFunc(&videoFrame);
    }
    
    return 0;
}

int DisplayController::fillAudioBuffer(uint8_t **buffersList, int lineCount, int oneLineSize, void *context){
    
    DisplayController *displayer = (DisplayController *)context;
    SyncClock *majorClock = displayer->getMajorClock();
    
    if (!displayer->shouldDisplay){
        return 0;
    }
    
    uint8_t *buffer = buffersList[0];
    if (displayer->paused) {
        printf("display pause audio\n");
        memset(buffer, 0, oneLineSize);
        return 0;
    }
    
    double startRealTime = (double)av_gettime_relative()/AV_TIME_BASE;
    
    TFMPDLOG_C("%.6f\n",startRealTime);
    
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
        uint8_t *dataBuffer = nullptr;
        int linesize = 0, outSamples = 0;
        
        TFMPAudioStreamDescription audioDesc = displayer->audioResampler.adoptedAudioDesc;
        int bytesPerSec = audioDesc.sampleRate*audioDesc.bitsPerChannel*audioDesc.channelsPerFrame/8;
        
        while (needReadSize > 0) {
            
            audioFrame = nullptr;
            
            if (displayer->shareAudioBuffer->isEmpty() || displayer->paused) {
                //fill remain buffer to 0.
                memset(buffer+(oneLineSize - needReadSize), 0, needReadSize);
                break;
            }else{
                if (displayer->checkingEnd && displayer->shareAudioBuffer->isEmpty()) {
                    
                    //回调给PlayController，返回true表明确实结束了，退出播放
                    if (displayer->encounterEndCallBack(displayer->encounterEndContext)) {
                        //剩余部分填充空值，结束
                        memset(buffer+(oneLineSize - needReadSize), 0, needReadSize);
                        return 0;
                    }
                }
                
                displayer->shareAudioBuffer->blockGetOut(&audioFrame);
            }
            
            if (audioFrame == nullptr) continue;
            if (audioFrame->serial != displayer->serial) {
                audioFrame->freeFrameFunc(&audioFrame);
                continue;
            }
            
            double pts = audioFrame->frame->pts*av_q2d(displayer->audioTimeBase);
            //使用了时间筛选，早于这个时间的都去掉
            if (displayer->filterTime && pts<displayer->filterTime) {
                audioFrame->freeFrameFunc(&audioFrame);
                continue;
            }
            
            //重采样
            AVFrame *frame = audioFrame->frame;
            if (displayer->audioResampler.isNeedResample(frame)) {
                if (displayer->audioResampler.reampleAudioFrame(frame, &outSamples, &linesize)) {
                    dataBuffer = displayer->audioResampler.resampledBuffers;
                }
            }else{
                dataBuffer = frame->extended_data[0];
                linesize = frame->linesize[0];
            }
            
            if (dataBuffer == nullptr) {
                audioFrame->freeFrameFunc(&audioFrame);
                continue;
            }
            
            int unplaySize = (oneLineSize-needReadSize)+linesize;
            double unplayDelay = (double)unplaySize/bytesPerSec+audioDesc.bufferDelay; //还未播的缓冲区数据的时间
            
            double remainTime = 0;
            if (majorClock->serial == displayer->serial) {
                remainTime = majorClock->getRemainTime(pts)-unplayDelay; //当前帧播放时间剩余
            }
            
            if (remainTime < -minExeTime){
                TFMPDLOG_C("slow: %.3f\n",remainTime);
                audioFrame->freeFrameFunc(&audioFrame);
                continue;
            }else if (remainTime > 0.5){
                //音频超前时不能直接阻塞线程，会导致播放不了（具体原因再看），插入空白数据段会更好，相当于一段静音
                //但是这个静音的时长需要斟酌，太短如0.01s会破坏原本的音频，太长会有明显的卡顿
                size_t slientSize = min((int)(bytesPerSec*remainTime), needReadSize);
                memset(buffer+(oneLineSize - needReadSize), 0, slientSize);
                needReadSize -= slientSize;
                
                unplayDelay += slientSize/bytesPerSec;
            }
            
            //当前内容的时间(pts)-未播的数据延迟(unplayDelay) = 刚播完的数据时间; 对应的现实时间传入方法刚调用的时间
            displayer->audioClock->updateTime(pts-unplayDelay, audioFrame->serial, startRealTime);
            
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
        }
    }
    
    return 0;
}

TFMPFillAudioBufferStruct DisplayController::getFillAudioBufferStruct(){
    return {fillAudioBuffer, this};
}
