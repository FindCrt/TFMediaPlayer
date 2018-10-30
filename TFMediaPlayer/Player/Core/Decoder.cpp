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
    
    TFMPPacket packet = {0, nullptr};
    bool packetPending = false;
    AVFrame *frame = nullptr;
    
    string name = decoder->name;
    while (decoder->shouldDecode) {
        
        //TODO: AVERROR_EOF
        int retval = AVERROR(EAGAIN);
        
        do {
            frame = av_frame_alloc();
            retval = avcodec_receive_frame(decoder->codecCtx, frame);
            
            if (retval == AVERROR(EAGAIN)){
                av_frame_free(&frame);
                break;
            }else if (retval != 0){
                av_frame_free(&frame);
            }
        } while (retval != 0);
        
        
        int loopCount = 0;
        do {
            loopCount++;
            if (packetPending) {  //上一轮packet有遗留，就先不取
                packetPending = false;
            }else{
                if (packet.pkt) av_packet_free(&(packet.pkt));
                decoder->pktBuffer.blockGetOut(&packet);
                if (packet.pkt == nullptr) break;
                if (decoder->timebase.den) {
                    TFMPDLOG_C("[%s]packet: %.6f, %d,%d\n",decoder->name.c_str(),packet.pkt->pts*av_q2d(decoder->timebase), packet.serial,decoder->serial);
                }
            }
        } while (packet.serial != decoder->serial);
        
        if (decoder->timebase.den) {
            decoder->pktBuffer.log();
        }
        
        if (loopCount > 1) {  //经历serial变化的阶段，之前的packet不可用
            avcodec_flush_buffers(decoder->codecCtx);
            if (frame) av_frame_free(&frame);
        }else if (frame) {
            //这一句和上面while判断之间，decoder的serial可能会变，但packet的serial不会变
            //没变时，两个值是一样的；变了，这个frame是更早的packet的数据，它的serial肯定是跟packet相同而不是跟decoder相同
            decoder->frameBuffer.blockInsert(tfmpFrameFromAVFrame(frame, true, packet.serial));
        }
        
        retval = avcodec_send_packet(decoder->codecCtx, packet.pkt);
        
        if (retval==AVERROR(EAGAIN)) { //现在接收不了数据，要先取frame
            packetPending = true;
        }else if (retval != 0 && packet.pkt){
            av_packet_free(&(packet.pkt));
        }
    }
    
    decoder->isDecoding = false;
    
    return 0;
}


