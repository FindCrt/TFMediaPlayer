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
        
        printf("remainTime: %.6f\n",remainTime);
        
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
//        if(showAudio) av_frame_unref(audioFrame);
    }
    
    return 0;
}

int DisplayController::fillAudioBuffer(uint8_t **buffersList, int lineCount, int oneLineSize, void *context){
    
    DisplayController *displayer = (DisplayController *)context;
    
    for (int i = 0; i<lineCount; i++) {
        
        uint8_t *buffer = buffersList[i];
        
        if (displayer->remainingSize[i] >= oneLineSize) {
            //TODO: do more thing for planar audio.
            memcpy(buffer, displayer->remainingAudioBuffer[i], oneLineSize);
            
            displayer->remainingSize[i] -= oneLineSize;
            displayer->remainingAudioBuffer[i] += oneLineSize;
            
        }else{
            
            int needReadSize = oneLineSize;
            if (displayer->remainingSize[i] > 0) {
                
                needReadSize -= displayer->remainingSize[i];
                memcpy(buffer, displayer->remainingAudioBuffer[i], displayer->remainingSize[i]);
                
                displayer->remainingAudioBuffer[i] = nullptr;
                displayer->remainingSize[i] = 0;
                
                //TODO: Maybe the frames of different line is different.We need to reatin/release different frames.
//                av_frame_free(&displayer->remainFrame); // release this frame
            }
            
            AVFrame *frame = av_frame_alloc();
            int frameLineSize = 0; //Each channel plane size is same for audio.
            
            uint8_t *dataBuffer[8];
            int linesize[8];
            
            while (needReadSize > 0) {
                
                displayer->shareAudioBuffer->blockGetOut(&frame);
                
                if (displayer->audioResampler->isNeedResample(frame)) {
                    displayer->audioResampler->reampleAudioFrame(frame, dataBuffer, linesize);
                }
                
                frameLineSize = frame->linesize[0];
                
                printf("one sample size: %d | %d, %d\n",frameLineSize/frame->nb_samples,frameLineSize, frame->nb_samples);
                
                if (needReadSize >= frameLineSize) {
                    
                    memcpy(buffer, frame->data[i], frameLineSize);
                    needReadSize -= frameLineSize;
                    
                }else{
                    
                    //there is a little buffer left.
                    displayer->remainFrame = av_frame_alloc(); //retain this frame
                    av_frame_ref(displayer->remainFrame, frame);
                    displayer->remainingSize[i] = frameLineSize - needReadSize;
                    displayer->remainingAudioBuffer[i] = frame->data[i] + needReadSize;
                    
                    memcpy(buffer, frame->data[i], needReadSize);
                    needReadSize = 0;
                }
            }
        }

    }
    
    return 0;
}

TFMPFillAudioBufferStruct DisplayController::getFillAudioBufferStruct(){
    return {fillAudioBuffer, this};
}




