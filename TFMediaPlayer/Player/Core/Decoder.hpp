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
#include <libavutil/time.h>
}

#include "RecycleBuffer.hpp"
#include <pthread.h>
#include "TFMPAVFormat.h"

namespace tfmpcore {
    
    class Decoder{
        
        AVFormatContext *fmtCtx;
        int steamIndex;
        
        AVCodecContext *codecCtx;
        
        RecycleBuffer<AVPacket*> pktBuffer = RecycleBuffer<AVPacket*>(100, false);
        
        RecycleBuffer<AVFrame*> frameBuffer = RecycleBuffer<AVFrame *>(100, false);
        
        pthread_t decodeThread;
        static void *decodeLoop(void *context);
        
        bool shouldDecode;
        bool isDecoding;
        
    public:
        AVMediaType type;
        Decoder(AVFormatContext *fmtCtx, int steamIndex, AVMediaType type):fmtCtx(fmtCtx),steamIndex(steamIndex),type(type){};
        
        ~Decoder(){
            freeResources();
        }
        
        RecycleBuffer<AVFrame*> *sharedFrameBuffer(){
            return &frameBuffer;
        };
        
        bool prepareDecode();
        
        void startDecode();
        void stopDecode();
        void freeResources();
        
        void decodePacket(AVPacket *packet);
    };
}

#endif /* Decoder_hpp */
