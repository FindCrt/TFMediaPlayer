//
//  Decoder.hpp
//  TFMediaPlayer
//
//  Created by shiwei on 17/12/28.
//  Copyright © 2017年 shiwei. All rights reserved.
//

#ifndef Decoder_hpp
#define Decoder_hpp

#include <stdio.h>
extern "C"{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

#include "RecycleBuffer.hpp"
#include <pthread.h>

namespace tfmpcore {
    class Decoder{
        
        AVFormatContext *fmtCtx;
        int steamIndex;
        
        AVCodecContext *codecCtx;
        
        RecycleBuffer<AVPacket> pktBuffer;
        
        RecycleBuffer<AVFrame> frameBuffer;
        
        pthread_t decodeThread;
        static void *decodeLoop(void *context);
        
    public:
        AVMediaType type;
        Decoder(AVFormatContext *fmtCtx, int steamIndex, AVMediaType type):fmtCtx(fmtCtx),steamIndex(steamIndex),type(type){};
        
        RecycleBuffer<AVFrame> *sharedFrameBuffer(){
            return &frameBuffer;
        };
        
        bool prepareDecode();
        
        void startDecode();
        void decodePacket(AVPacket *packet);
        
    };
}

#endif /* Decoder_hpp */
