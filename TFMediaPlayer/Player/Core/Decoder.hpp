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
        
        AVFormatContext *fmtCtx;
        int steamIndex;
        
        AVCodecContext *codecCtx;
        
        RecycleBuffer<TFMPPacket> pktBuffer = RecycleBuffer<TFMPPacket>(2000, true);
        RecycleBuffer<TFMPFrame*> frameBuffer = RecycleBuffer<TFMPFrame *>(30, true);
        
        pthread_t decodeThread;
        static void *decodeLoop(void *context);
        
        bool shouldDecode = false;
        
        inline static void freePacket(TFMPPacket *packet){
            av_packet_free(&(packet->pkt));
            packet->pkt = nullptr;
        }
        
        inline static void freeFrame(TFMPFrame **tfmpFrameP){
            TFMPFrame *tfmpFrame = *tfmpFrameP;
            av_frame_free(&tfmpFrame->frame);
            
            delete tfmpFrame->displayBuffer;
            
            delete tfmpFrame;
            *tfmpFrameP = nullptr;
        }
        
        static TFMPVideoFrameBuffer *displayBufferFromFrame(TFMPFrame *tfmpFrame);
        static TFMPFrame *tfmpFrameFromAVFrame(AVFrame *frame, bool isAudio, int serial);
        
    public:
#if DEBUG
        string name;
        AVRational timebase;
#endif
        
        AVMediaType type;
        Decoder(AVFormatContext *fmtCtx, int steamIndex, AVMediaType type):fmtCtx(fmtCtx),steamIndex(steamIndex),type(type){};
        
        RecycleBuffer<TFMPFrame*> *sharedFrameBuffer(){
            return &frameBuffer;
        };
        
        int serial = 0;
        
        bool prepareDecode();
        void startDecode();
        void stopDecode();
        
        void insertPacket(AVPacket *packet);
    };
}

#endif /* Decoder_hpp */
