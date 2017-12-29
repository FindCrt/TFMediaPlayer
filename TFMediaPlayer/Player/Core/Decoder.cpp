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
    int retval = avcodec_open2(codecCtx, codec, NULL);
    if (retval < 0) {
        printf("avcodec_open2 id: %d error\n",codec->id);
        return false;
    }
    
    return true;
}

void Decoder::startDecode(){
    pthread_create(&decodeThread, NULL, decodeLoop, this);
}

void Decoder::decodePacket(AVPacket *packet){
    
    AVPacket refPacket;
    av_packet_ref(&refPacket, packet);
    
    pktBuffer.insert(refPacket);
}

void *Decoder::decodeLoop(void *context){
    
    Decoder *decoder = (Decoder *)context;
    
    AVPacket pkt;
    AVFrame frame;
    
    while (decoder->pktBuffer.getOut(&pkt)) {
        int retval = avcodec_send_packet(decoder->codecCtx, &pkt);
        if (retval != 0) {
            printf("avcodec_send_packet\n");
            continue;
        }
        
        AVFrame refFrame;
        av_frame_ref(&refFrame, &frame);
        avcodec_receive_frame(decoder->codecCtx, &refFrame);
        
        decoder->frameBuffer.insert(frame);
        av_packet_unref(&pkt);
    }
    
    return 0;
}
