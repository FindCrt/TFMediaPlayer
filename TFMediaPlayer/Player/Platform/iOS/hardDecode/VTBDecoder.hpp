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

using namespace std;

//An video & audio decoder based on VideoToolBox and ffmpeg.

namespace tfmpcore {
    
    class VTBFrame{
        CVPixelBufferRef pixelBuffer = nullptr;
        
    public:
        VTBFrame(CVPixelBufferRef pixelBuffer):pixelBuffer(pixelBuffer){};
        bool key_frame;
        uint64_t pts;
        
        static void free(VTBFrame **frame){
            
        }
        
        TFMPVideoFrameBuffer *convertToTFMPBuffer();
    };
    
    class VTBDecoder{
        
        RecycleBuffer<AVPacket*> pktBuffer = RecycleBuffer<AVPacket*>(50, true);
        RecycleBuffer<VTBFrame*> frameBuffer = RecycleBuffer<VTBFrame*>(50, true);
        
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
        
        bool initDecoder();
        void decodePacket(AVPacket *pkt);
        
        void static decodeCallback(void * CM_NULLABLE decompressionOutputRefCon,void * CM_NULLABLE sourceFrameRefCon,OSStatus status,VTDecodeInfoFlags infoFlags,CM_NULLABLE CVImageBufferRef imageBuffer,CMTime presentationTimeStamp,CMTime presentationDuration );
        
    protected:
        void flushContext();
        
    public:
        string name;
        AVMediaType type;
        RecycleBuffer<VTBFrame*> * sharedFrameBuffer(){
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
