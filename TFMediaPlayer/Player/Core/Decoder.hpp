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
        
        RecycleBuffer<AVPacket*> pktBuffer = RecycleBuffer<AVPacket*>(30, true);
        
        RecycleBuffer<AVFrame*> frameBuffer = RecycleBuffer<AVFrame *>(30, true);
        
        pthread_t decodeThread;
        static void *decodeLoop(void *context);
        
        bool shouldDecode = false;
        /** decode loop is running */
        bool isDecoding = false;
        
        bool pause = false;
        
        pthread_cond_t waitLoopCond = PTHREAD_COND_INITIALIZER;
        pthread_mutex_t waitLoopMutex = PTHREAD_MUTEX_INITIALIZER;
        
        pthread_cond_t pauseCond = PTHREAD_COND_INITIALIZER;
        pthread_mutex_t pauseMutex = PTHREAD_MUTEX_INITIALIZER;
        
    public:
        string name;
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
