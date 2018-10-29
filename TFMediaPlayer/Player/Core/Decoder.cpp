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

TFMPVideoFrameBuffer * Decoder::displayBufferFromFrame(TFMPFrame *tfmpFrame){
    TFMPVideoFrameBuffer *displayFrame = new TFMPVideoFrameBuffer();
    
    AVFrame *frame = tfmpFrame->frame;
    displayFrame->width = frame->width;
    //TODO: when should i cut one line of data to avoid the green data-empty zone in bottom?
    displayFrame->height = frame->height-1;
    
    for (int i = 0; i<AV_NUM_DATA_POINTERS; i++) {
        
        displayFrame->pixels[i] = frame->data[i]+frame->linesize[i];
        displayFrame->linesize[i] = frame->linesize[i];
    }
    
    //TODO: unsupport format
    if (frame->format == AV_PIX_FMT_YUV420P) {
        displayFrame->format = TFMP_VIDEO_PIX_FMT_YUV420P;
    }else if (frame->format == AV_PIX_FMT_NV12){
        displayFrame->format = TFMP_VIDEO_PIX_FMT_NV12;
    }else if (frame->format == AV_PIX_FMT_NV21){
        displayFrame->format = TFMP_VIDEO_PIX_FMT_NV21;
    }else if (frame->format == AV_PIX_FMT_RGB32){
        displayFrame->format = TFMP_VIDEO_PIX_FMT_RGB32;
    }
    
    return displayFrame;
}

TFMPFrame * Decoder::tfmpFrameFromAVFrame(AVFrame *frame, bool isAudio, int serial){
    TFMPFrame *tfmpFrame = new TFMPFrame();
    
    tfmpFrame->serial = serial;
    tfmpFrame->frame  = frame;
    tfmpFrame->type = isAudio ? TFMPFrameTypeAudio:TFMPFrameTypeVideo;
    tfmpFrame->freeFrameFunc = Decoder::freeFrame;
    tfmpFrame->pts = frame->pts;
    tfmpFrame->displayBuffer = displayBufferFromFrame(tfmpFrame);
    
    return tfmpFrame;
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
    
//    string stateName = name+" flush";
//
//    //1. prevent from starting next decode loop
//    pause = true;
//
//    //2. disable buffer's in and out to invalid the frame or packet in current loop.
//
//    pktBuffer.disableIO(true);
//    frameBuffer.disableIO(true);
//
//
//    //3. wait for the end of current loop
//    pthread_mutex_lock(&waitLoopMutex);
//    if (isDecoding) {
//
//        pthread_cond_wait(&waitLoopCond, &waitLoopMutex);
//    }else{
//
//    }
//    pthread_mutex_unlock(&waitLoopMutex);
//
//
//    //4. flush all reserved buffers
//    pktBuffer.flush();
//
//    frameBuffer.flush();
//
//
//    //5. flush FFMpeg's buffer.
//    //If dont'f call this, there are some new packets which contains old frames.
//    avcodec_flush_buffers(codecCtx);
//
//
//    //6. resume the decode loop
//    pktBuffer.disableIO(false);
//    frameBuffer.disableIO(false);
//
//    pause = false;
//
//    TFMPCondSignal(pauseCond, pauseMutex);
    
    
    
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
    
    pthread_mutex_lock(&waitLoopMutex);
    if (isDecoding) {
        pthread_cond_wait(&waitLoopCond, &waitLoopMutex);
        
    }else{
        
    }
    pthread_mutex_unlock(&waitLoopMutex);
    
    //4. flush all reserved buffers
    pktBuffer.flush();
    frameBuffer.flush();
    
    if (codecCtx) avcodec_free_context(&codecCtx);
    
    fmtCtx = nullptr;
}

void Decoder::insertPacket(AVPacket *packet){
    pktBuffer.blockInsert({serial, packet});
}

void *Decoder::decodeLoop(void *context){
    
    Decoder *decoder = (Decoder *)context;
    
    decoder->isDecoding = true;
    
    TFMPPacket packet;
    AVFrame *frame = av_frame_alloc();
    
    bool frameDelay = false;
    bool delayFramesReleasing = false;
    
    string name = decoder->name;
    while (decoder->shouldDecode) {
        
        packet.pkt = nullptr;
        decoder->pktBuffer.blockGetOut(&packet);
        
        if (packet.pkt == nullptr || packet.serial != decoder->serial) {
            continue;
        }
        
        int retval = avcodec_send_packet(decoder->codecCtx, packet.pkt);
        TFMPDLOG_C("packet: %.6f, %d,%d\n",packet.pkt->pts*av_q2d(decoder->timebase), packet.serial,retval);
        if (retval < 0) {
            TFCheckRetval("avcodec send packet");
            
            av_packet_free(&packet.pkt);
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
                
                if (decoder->shouldDecode && packet.serial == decoder->serial) {
                    AVFrame *refFrame = av_frame_alloc();
                    av_frame_ref(refFrame, frame);
                    decoder->frameBuffer.blockInsert(tfmpFrameFromAVFrame(refFrame, true, decoder->serial));
                }else{
                    av_frame_unref(frame);
                }
            }
        }else{
            
            //frame type: i p     b b b b            p                b b              p
            //status:    normal->frameDelay->delayFramesReleasing->frameDelay->delayFramesReleasing
            
            int releaseTime = 0;
            do {
                
                releaseTime++;
                retval = avcodec_receive_frame(decoder->codecCtx, frame);
                
                if (retval == AVERROR(EAGAIN)) {  //encounter B-frame
                    
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
                    
                    delayFramesReleasing = true;
                    frameDelay = false;
                }
                

                
                if (frame->extended_data == nullptr) {
                    printf("video frame data is null\n");
                    av_frame_unref(frame);
                    continue;
                }
                
                if (!decoder->mediaTimeFilter->checkFrame(frame, false)) {
                    
                    av_frame_unref(frame);
                    continue;
                }
                
                if (decoder->shouldDecode && packet.serial == decoder->serial) {
                    AVFrame *refFrame = av_frame_alloc();
                    av_frame_ref(refFrame, frame);
                    TFMPDLOG_C("[%d,%d]%.6f\n",packet.serial,decoder->serial,frame->pts*av_q2d(decoder->timebase));
                    decoder->frameBuffer.blockInsert(tfmpFrameFromAVFrame(refFrame, false, decoder->serial));
                    
                }else{
                    av_frame_unref(frame);
                }
            } while (delayFramesReleasing);
        }
        
        if (packet.pkt != nullptr)  av_packet_free(&(packet.pkt));
    }
    
    
    av_frame_free(&frame);
    decoder->isDecoding = false;
    TFMPCondSignal(decoder->waitLoopCond, decoder->waitLoopMutex);
    
    return 0;
}


