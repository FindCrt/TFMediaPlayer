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
#include <vector>
#include "MediaTimeFilter.hpp"

namespace tfmpcore {
    
    class Decoder{
        
        AVFormatContext *fmtCtx;
        int steamIndex;
        
        AVCodecContext *codecCtx;
        
        RecycleBuffer<AVPacket*> pktBuffer = RecycleBuffer<AVPacket*>(5, true);
        
        RecycleBuffer<AVFrame*> frameBuffer = RecycleBuffer<AVFrame *>(50, true);
        
        pthread_t decodeThread;
        static void *decodeLoop(void *context);
        
        bool shouldDecode = false;
        bool isDecoding = false;
        
        bool pause = false;
        
    public:
        AVMediaType type;
        Decoder(AVFormatContext *fmtCtx, int steamIndex, AVMediaType type):fmtCtx(fmtCtx),steamIndex(steamIndex),type(type){};
        
        ~Decoder(){
            freeResources();
        }
        
        RecycleBuffer<AVFrame*> *sharedFrameBuffer(){
            return &frameBuffer;
        };
        
        MediaTimeFilter *mediaTimeFilter;
        
        bool prepareDecode();
        
        void startDecode();
        void stopDecode();
        
        void flush();
        void freeResources();
        
        bool bufferIsEmpty();
        
        void decodePacket(AVPacket *packet);
        
    };
}

#endif /* Decoder_hpp */
