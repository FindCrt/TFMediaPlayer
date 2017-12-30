//
//  Decoder.cpp
//  TFMediaPlayer
//
//  Created by shiwei on 17/12/28.
//  Copyright © 2017年 shiwei. All rights reserved.
//

#include "Decoder.hpp"
#include "DebugFuncs.h"


using namespace tfmpcore;

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
    
    shouldDecode = true;
    
    return true;
}

void Decoder::startDecode(){
    pthread_create(&decodeThread, NULL, decodeLoop, this);
}

void Decoder::stopDecode(){
    shouldDecode = false;
}

void Decoder::decodePacket(AVPacket *packet){
    
    AVPacket *refPacket = av_packet_alloc();
    av_packet_ref(refPacket, packet);
    
    pktBuffer.blockInsert(*refPacket);
}

void *Decoder::decodeLoop(void *context){
    
    Decoder *decoder = (Decoder *)context;
    
    AVPacket pkt;
    AVFrame *frame = av_frame_alloc();
    
    while (decoder->shouldDecode) {
        
        decoder->pktBuffer.blockGetOut(&pkt);
        int retval = avcodec_send_packet(decoder->codecCtx, &pkt);
        if (retval != 0) {
            printf("avcodec_send_packet\n");
            continue;
        }
        
        retval = avcodec_receive_frame(decoder->codecCtx, frame);
        
        
        if (retval != 0) {
            printf("avcodec_receive_frame\n");
            continue;
        }
        
        decoder->frameBuffer.blockInsert(*frame);
        av_packet_unref(&pkt);
    }
    
    return 0;
}
