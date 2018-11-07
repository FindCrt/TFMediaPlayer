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

#include "TFMPFrame.h"
#include "TFMPPacket.h"
#include "RecycleBuffer.hpp"
#include <pthread.h>
#include "TFMPAVFormat.h"

namespace tfmpcore {
    
    class Decoder{
        
    protected:
        AVFormatContext *fmtCtx;
        int streamIndex;
        
        AVCodecContext *codecCtx;
        
        RecycleBuffer<TFMPPacket> pktBuffer = RecycleBuffer<TFMPPacket>(2000, true);
        RecycleBuffer<TFMPFrame*> frameBuffer = RecycleBuffer<TFMPFrame *>(30, true);
        
        pthread_t decodeThread;
        
        bool shouldDecode = false;
        
        void *(*decodeLoopFunc)(void *context);
        
        inline static void freePacket(TFMPPacket *packet){
            av_packet_free(&(packet->pkt));
            packet->pkt = nullptr;
        }
        
    public:
#if DEBUG
        string name;
        AVRational timebase;
#endif
        
        AVMediaType type;
        void init(AVFormatContext *fmtCtx, int streamIndex, AVMediaType type){
            this->fmtCtx = fmtCtx;
            this->streamIndex = streamIndex;
            this->type = type;
        };
        
        RecycleBuffer<TFMPFrame*> *sharedFrameBuffer(){
            return &frameBuffer;
        };
        
        int serial = 0;
        
        virtual bool prepareDecode() = 0;
        virtual void startDecode();
        virtual void stopDecode();
        
        virtual void insertPacket(AVPacket *packet);
        
        bool isEmpty();
    };
}

#endif /* Decoder_hpp */
