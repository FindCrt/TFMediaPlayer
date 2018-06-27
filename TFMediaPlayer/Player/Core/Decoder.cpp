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
    printf("freePacket\n");
    av_packet_free(pkt);
}

inline void freeFrame(AVFrame **frame){
    printf("freeFrame\n");
//    av_usleep(100000);

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

void Decoder::flush(){
    TFMPDLOG_C("Decoder::flush");
    //1. prevent from starting next decode loop
    pause = true;
    
    //2. disable buffer's in and out to invalid the frame or packet in current loop.
    pktBuffer.disableIO(true);
    frameBuffer.disableIO(true);
    
    //3. wait for the end of current loop
    TFMPCondWait(waitLoopCond, waitLoopMutex)
    TFMPDLOG_C("flush wait end");
    
    //4. flush all reserved buffers
    pktBuffer.flush();
    frameBuffer.flush();
    
    //5. flush FFMpeg's buffer.
    //If dont'f call this, there are some new packets which contains old frames.
    avcodec_flush_buffers(codecCtx);
    
    //6. resume the decode loop
    pktBuffer.disableIO(false);
    frameBuffer.disableIO(false);
    
    pause = false;
    TFMPCondSignal(pauseCond)
    
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
    TFMPCondWait(waitLoopCond, waitLoopMutex)
    
    //4. flush all reserved buffers
    pktBuffer.flush();
    frameBuffer.flush();
    
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
    
    AVPacket *pkt = nullptr;
    AVFrame *frame = av_frame_alloc();
    
    bool frameDelay = false;
    bool delayFramesReleasing = false;
    
    while (decoder->shouldDecode) {
        
        if (decoder->pause) {
            TFMPDLOG_C("decoder pause");
            TFMPCondSignal(decoder->waitLoopCond);
            TFMPCondWait(decoder->pauseCond, decoder->pauseMutex);
        }
        
        pkt = nullptr;
        decoder->pktBuffer.blockGetOut(&pkt);
        if (pkt == nullptr) {
            TFMPDLOG_C("pkt null");
        }
        if (pkt == nullptr) continue;
        
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
                
                if (decoder->shouldDecode) {
                    AVFrame *refFrame = av_frame_alloc();
                    av_frame_ref(refFrame, frame);
//                    av_usleep(50000);
                    TFMPDLOG_C("insert audio frame: %lld,%lld\n",pkt->pts,refFrame->pts);
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
                    decoder->frameBuffer.blockInsert(refFrame);
                    TFMPDLOG_C("insert video frame2: %lld,%lld\n",pkt->pts,refFrame->pts);
                }else{
                    av_frame_unref(frame);
                }
            } while (delayFramesReleasing);
        }
        
        if (pkt != nullptr)  av_packet_free(&pkt);
    }
    TFMPDLOG_C("decode end");
    av_frame_free(&frame);
    TFMPCondSignal(decoder->waitLoopCond);
    return 0;
}


