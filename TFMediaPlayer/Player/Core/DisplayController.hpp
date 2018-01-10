//
//  DisplayController.hpp
//  TFMediaPlayer
//
//  Created by shiwei on 17/12/29.
//  Copyright © 2017年 shiwei. All rights reserved.
//

#ifndef DisplayController_hpp
#define DisplayController_hpp

#include <stdlib.h>
#include <pthread.h>
#include "TFMPAVFormat.h"
#include "RecycleBuffer.hpp"
#include "SyncClock.hpp"

extern "C"{
#include <libavformat/avformat.h>
}

namespace tfmpcore {
    
    class DisplayController{
        
        pthread_t dispalyThread;
        static void *displayLoop(void *context);
        
        bool shouldDisplay;
        
        uint64_t remainingSize;
        uint8_t *remainingAudioBuffer;
        AVFrame *remainFrame;
        
        static int fillAudioBuffer(void *buffer, int size, void *context);
        
    public:
        
        TFMPMediaType displayMediaType = TFMP_MEDIA_TYPE_ALL_AVIABLE;
        
        //the real display func different with different platform
        void *displayContext;
        
        TFMPVideoFrameDisplayFunc displayVideoFrame;
        TFMPFillAudioBufferStruct getFillAudioBufferStruct();
        
        RecycleBuffer<AVFrame*> *shareVideoBuffer;
        RecycleBuffer<AVFrame*> *shareAudioBuffer;
        
        AVRational videoTimeBase;
        AVRational audioTimeBase;
        
        SyncClock *syncClock;
        

        void startDisplay();
        void stopDisplay();
        
        void freeResource();
    };
}

#endif /* DisplayController_hpp */
