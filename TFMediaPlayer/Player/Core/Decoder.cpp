//
//  Decoder.cpp
//  TFMediaPlayer
//
//  Created by shiwei on 17/12/28.
//  Copyright © 2017年 shiwei. All rights reserved.
//

#include "Decoder.hpp"
#include "TFMPDebugFuncs.h"
#include "TFMPUtilities.h"
#include <vector>
#include <atomic>
#include <iostream>

#if DEBUG
#include "FFmpegInternalDebug.h"
#endif

using namespace tfmpcore;

inline void freePacket(AVPacket **pkt){
    av_packet_free(pkt);
}

inline void freeFrame(AVFrame **frame){
    av_frame_free(frame);
}

bool Decoder::prepareDecode(){
    AVCodec *codec = avcodec_find_decoder(fmtCtx->streams[steamIndex]->codecpar->codec_id);
    if (codec == nullptr) {
        printf("find codec type: %d error\n",type);
        return false;
    }
    
    codecCtx = avcodec_alloc_context3(codec);
    if (codecCtx == nullptr) {
        printf("alloc codecContext type: %d error\n",type);
        return false;
    }
    
    avcodec_parameters_to_context(codecCtx, fmtCtx->streams[steamIndex]->codecpar);
    
    int retval = avcodec_open2(codecCtx, codec, NULL);
    if (retval < 0) {
        printf("avcodec_open2 id: %d error\n",codec->id);
        return false;
    }
    
#if DEBUG
    if (type == AVMEDIA_TYPE_AUDIO) {
        strcpy(frameBuffer.name, "audio_frame");
        strcpy(pktBuffer.name, "audio_packet");
    }else if (type == AVMEDIA_TYPE_VIDEO){
        strcpy(frameBuffer.name, "video_frame");
        strcpy(pktBuffer.name, "video_packet");
    }else{
        strcpy(frameBuffer.name, "subtitle_frame");
        strcpy(pktBuffer.name, "subtitle_packet");
    }
#endif
    
    shouldDecode = true;
    
    pktBuffer.valueFreeFunc = freePacket;
    frameBuffer.valueFreeFunc = freeFrame;
    
    RecycleBuffer<TFMPMediaType> intBuffer;
    TFMPMediaType a;
    intBuffer.getOut(&a);
    
    return true;
}

void Decoder::startDecode(){
    pthread_create(&decodeThread, NULL, decodeLoop, this);
    pthread_detach(decodeThread);
}

void Decoder::stopDecode(){
    shouldDecode = false;
}

void Decoder::activeBlock(bool flag){
    pktBuffer.disableIO(!flag);
    frameBuffer.disableIO(!flag);
}

void Decoder::flush(){
    
    string stateName = name+" flush";
    
    //1. prevent from starting next decode loop
    pause = true;
    
    //2. disable buffer's in and out to invalid the frame or packet in current loop.
    myStateObserver.mark(stateName, 1);
    pktBuffer.disableIO(true);
    frameBuffer.disableIO(true);
    myStateObserver.mark(stateName, 1);
    
    //3. wait for the end of current loop
    pthread_mutex_lock(&waitLoopMutex);
    if (isDecoding) {
        myStateObserver.mark(stateName, 3);
        pthread_cond_wait(&waitLoopCond, &waitLoopMutex);
    }else{
        myStateObserver.mark(stateName, 4);
    }
    pthread_mutex_unlock(&waitLoopMutex);
    myStateObserver.mark(stateName, 5);
    
    //4. flush all reserved buffers
    pktBuffer.flush();
    myStateObserver.mark(stateName, 6);
    frameBuffer.flush();
    myStateObserver.mark(stateName, 7);
    
    //5. flush FFMpeg's buffer.
    //If dont'f call this, there are some new packets which contains old frames.
    avcodec_flush_buffers(codecCtx);
    myStateObserver.mark(stateName, 8);
    
    //6. resume the decode loop
    pktBuffer.disableIO(false);
    frameBuffer.disableIO(false);
    
    pause = false;
    myStateObserver.mark(stateName, 9);
    TFMPCondSignal(pauseCond, pauseMutex);
    myStateObserver.mark(stateName, 10);
    
    TFMPDLOG_C("flush end %s\n",frameBuffer.name);
}

bool Decoder::bufferIsEmpty(){
    return pktBuffer.isEmpty() && frameBuffer.isEmpty();
}

void Decoder::freeResources(){
    shouldDecode = false;
    
    //2. disable buffer's in and out to invalid the frame or packet in current loop.
    pktBuffer.disableIO(true);
    frameBuffer.disableIO(true);
    //3. wait for the end of current loop
    myStateObserver.mark(name+" free", 1);
    pthread_mutex_lock(&waitLoopMutex);
    if (isDecoding) {
        pthread_cond_wait(&waitLoopCond, &waitLoopMutex);
        myStateObserver.mark(name+" free", 2);
    }else{
        myStateObserver.mark(name+" free", 3);
    }
    pthread_mutex_unlock(&waitLoopMutex);
    myStateObserver.mark(name+" free", 4);
    //4. flush all reserved buffers
    pktBuffer.flush();
    frameBuffer.flush();
    myStateObserver.mark(name+" free", 5);
    if (codecCtx) avcodec_free_context(&codecCtx);
    
    fmtCtx = nullptr;
}

void Decoder::decodePacket(AVPacket *packet){
    
    AVPacket *refPkt = av_packet_alloc();
    av_packet_ref(refPkt, packet);
    
    pktBuffer.blockInsert(refPkt);
}

void *Decoder::decodeLoop(void *context){
    
    Decoder *decoder = (Decoder *)context;
    
    decoder->isDecoding = true;
    
    AVPacket *pkt = nullptr;
    AVFrame *frame = av_frame_alloc();
    
    bool frameDelay = false;
    bool delayFramesReleasing = false;
    
    string name = decoder->name;
    while (decoder->shouldDecode) {
        
        if (decoder->pause) {
            myStateObserver.mark(name, 1);
            decoder->isDecoding = false;
            TFMPCondSignal(decoder->waitLoopCond, decoder->waitLoopMutex);
            myStateObserver.mark(name, 11);
            
            pthread_mutex_lock(&decoder->pauseMutex);
            if (decoder->pause) {
                myStateObserver.mark(name, 12);
                pthread_cond_wait(&decoder->pauseCond, &decoder->pauseMutex);
            }else{
                myStateObserver.mark(name, 13);
            }
            pthread_mutex_unlock(&decoder->pauseMutex);
            decoder->isDecoding = true;
        }
        
        pkt = nullptr;
        myStateObserver.mark(name, 2);
        decoder->pktBuffer.blockGetOut(&pkt);
        myStateObserver.mark(name, 3);
        if (pkt == nullptr) {
            TFMPDLOG_C("pkt null");
        }
        if (pkt == nullptr) continue;
        
        myStateObserver.mark(name, 4);
        int retval = avcodec_send_packet(decoder->codecCtx, pkt);
        if (retval < 0) {
            TFCheckRetval("avcodec send packet");
            TFMPDLOG_C("pkt: %x\n",pkt);
            av_packet_free(&pkt);
            continue;
        }
        
        if (decoder->type == AVMEDIA_TYPE_AUDIO) {
            
            //may many frames in one packet.
            while (retval == 0) {
                myStateObserver.mark(name, 6);
                retval = avcodec_receive_frame(decoder->codecCtx, frame);
                if (retval == AVERROR(EAGAIN)) {
                    break;
                }
                
                if (retval != 0 && retval != AVERROR_EOF) {
                    TFCheckRetval("avcodec receive frame");
                    av_frame_unref(frame);
                    continue;
                }
                if (frame->extended_data == nullptr) {
                    printf("audio frame data is null\n");
                    av_frame_unref(frame);
                    continue;
                }
                
                if (!decoder->mediaTimeFilter->checkFrame(frame, false)) {
                    av_frame_unref(frame);
                    continue;
                }
                myStateObserver.mark(name, 7);
                if (decoder->shouldDecode) {
                    AVFrame *refFrame = av_frame_alloc();
                    av_frame_ref(refFrame, frame);
//                    av_usleep(50000);
                    myStateObserver.mark(name, 8);
                    if (decoder->frameBuffer.isEmpty()) {
                        myStateObserver.labelMark("audio first", to_string(refFrame->pts*av_q2d(decoder->timebase)));
                    }
                    decoder->frameBuffer.blockInsert(refFrame);
                    
                }else{
                    av_frame_unref(frame);
                }
            }
        }else{
            
            //frame type: i p     b b b b            p                b b              p
            //status:    normal->frameDelay->delayFramesReleasing->frameDelay->delayFramesReleasing
            
            int releaseTime = 0;
            do {
                TFMPDLOG_C("delayFramesReleasing\n");
                releaseTime++;
                retval = avcodec_receive_frame(decoder->codecCtx, frame);
                
                if (retval == AVERROR(EAGAIN)) {  //encounter B-frame
                    TFMPDLOG_C("encounter B-frame\n");
                    frameDelay = true;
                    delayFramesReleasing = false;
                    av_frame_unref(frame);
                    break;
                }else if (retval != 0) {  //other error
                    TFCheckRetval("avcodec receive frame");
                    delayFramesReleasing = false;
                    av_frame_unref(frame);
                    break;
                }
                
                if (frameDelay) {
                    TFMPDLOG_C("delayFramesReleasing\n");
                    delayFramesReleasing = true;
                    frameDelay = false;
                }
                
//                TFMPDLOG_C("[%d]frame Type: %d",releaseTime,frame->pict_type);
                
                if (frame->extended_data == nullptr) {
                    printf("video frame data is null\n");
                    av_frame_unref(frame);
                    continue;
                }
                
                if (!decoder->mediaTimeFilter->checkFrame(frame, false)) {
                    TFMPDLOG_C("video checkFrame\n");
                    av_frame_unref(frame);
                    continue;
                }
                
                if (decoder->shouldDecode) {
                    AVFrame *refFrame = av_frame_alloc();
                    av_frame_ref(refFrame, frame);
                    if (decoder->frameBuffer.isEmpty()) {
                        myStateObserver.labelMark("video first", to_string(refFrame->pts*av_q2d(decoder->timebase)));
                    }
                    decoder->frameBuffer.blockInsert(refFrame);
                    TFMPDLOG_C("insert video frame2: %lld,%lld\n",pkt->pts,refFrame->pts);
                }else{
                    av_frame_unref(frame);
                }
            } while (delayFramesReleasing);
        }
        
        if (pkt != nullptr)  av_packet_free(&pkt);
    }
    
    myStateObserver.mark(name, 9);
    av_frame_free(&frame);
    decoder->isDecoding = false;
    TFMPCondSignal(decoder->waitLoopCond, decoder->waitLoopMutex);
    myStateObserver.mark(name, 10);
    return 0;
}


