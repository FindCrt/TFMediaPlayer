//
//  VTBDecoder.hpp
//  TFMediaPlayer
//
//  Created by shiwei on 2018/9/28.
//  Copyright © 2018年 shiwei. All rights reserved.
//

#ifndef VTBDecoder_hpp
#define VTBDecoder_hpp

extern "C"{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/time.h>
}

#include <stdio.h>
#include <string>
#include <VideoToolbox/VideoToolbox.h>

#include "RecycleBuffer.hpp"
#include "MediaTimeFilter.hpp"
#include "TFMPAVFormat.h"
#include "TFMPFrame.h"

using namespace std;

//An video & audio decoder based on VideoToolBox and ffmpeg.
namespace tfmpcore {
    
    class VTBDecoder{
        
        AVFormatContext *fmtCtx;
        int steamIndex;
        AVCodecContext *codecCtx;
        AVRational timebase;
        
        RecycleBuffer<AVPacket*> pktBuffer = RecycleBuffer<AVPacket*>(30, true);
        RecycleBuffer<TFMPFrame*> frameBuffer = RecycleBuffer<TFMPFrame*>(30, true);
        
        pthread_t decodeThread;
        static void *decodeLoop(void *context);
        
        bool shouldDecode = false;
        
        bool isDecoding = false;
        
        bool pause = false;
        
        pthread_cond_t waitLoopCond = PTHREAD_COND_INITIALIZER;
        pthread_mutex_t waitLoopMutex = PTHREAD_MUTEX_INITIALIZER;
        
        pthread_cond_t pauseCond = PTHREAD_COND_INITIALIZER;
        pthread_mutex_t pauseMutex = PTHREAD_MUTEX_INITIALIZER;
        
        
        VTDecompressionSessionRef _decodeSession;
        CMFormatDescriptionRef _videoFmtDesc;
        
        uint8_t *_sps;
        uint8_t *_pps;
        uint32_t _spsSize = 0;
        uint32_t _ppsSize = 0;
        
        void decodePacket(AVPacket *pkt);
        
        void static decodeCallback(void * CM_NULLABLE decompressionOutputRefCon,void * CM_NULLABLE sourceFrameRefCon,OSStatus status,VTDecodeInfoFlags infoFlags,CM_NULLABLE CVImageBufferRef imageBuffer,CMTime presentationTimeStamp,CMTime presentationDuration );
        
        inline static void freePacket(AVPacket **pkt){
            av_packet_free(pkt);
        }
        
        inline static void freeFrame(TFMPFrame **frameP){
            TFMPFrame *frame = *frameP;
            
            CVPixelBufferRef pixelBuffer = (CVPixelBufferRef)frame->displayBuffer->opaque;
            CVPixelBufferRelease(pixelBuffer);
            
            delete frame->displayBuffer;
            
            delete frame;
            *frameP = nullptr;
            myStateObserver.mark("VTBFrame", -1, true);
        }
        
        inline static int frameCompare(TFMPFrame *&frame1, TFMPFrame *&frame2){
            if (frame1->pts < frame2->pts) {
                return -1;
            }else{
                return 1;
            }
        }
        
        static TFMPVideoFrameBuffer *displayBufferFromPixelBuffer(CVPixelBufferRef pixelBuffer);
        
    protected:
        void flushContext();
        
    public:
        string name;
        AVMediaType type;
        VTBDecoder(AVFormatContext *fmtCtx, int steamIndex, AVMediaType type):fmtCtx(fmtCtx),steamIndex(steamIndex),type(type){
            timebase = fmtCtx->streams[steamIndex]->time_base;
        };
        
        RecycleBuffer<TFMPFrame*> * sharedFrameBuffer(){
            return &frameBuffer;
        };
        
        MediaTimeFilter * mediaTimeFilter;
        
        bool prepareDecode();
        void startDecode();
        void stopDecode();
        
        void insertPacket(AVPacket *packet);
        
        bool bufferIsEmpty();
        
        void activeBlock(bool flag);
        void flush();
        void freeResources();
    };
}

#endif /* VTBDecoder_hpp */
