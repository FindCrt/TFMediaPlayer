//
//  DisplayController.hpp
//  TFMediaPlayer
//
//  Created by shiwei on 17/12/29.
//  Copyright © 2017年 shiwei. All rights reserved.
//

#ifndef DisplayController_hpp
#define DisplayController_hpp

#include <stdio.h>
#include <pthread.h>
#include "VideoFormat.h"
#include "RecycleBuffer.hpp"
#include "SyncClock.hpp"

extern "C"{
#include <libavformat/avformat.h>
}

namespace tfmpcore {
    
    typedef enum{
        TFMP_MEDIA_TYPE_VIDEO = 1 << 0,
        TFMP_MEDIA_TYPE_AUDIO = 1 << 1,
        TFMP_MEDIA_TYPE_SUBTITLE = 1 << 2,
        TFMP_MEDIA_TYPE_ALL_AVIABLE = TFMP_MEDIA_TYPE_VIDEO | TFMP_MEDIA_TYPE_AUDIO | TFMP_MEDIA_TYPE_SUBTITLE,
    }TFMPMediaType;
    
    typedef int (*VideoFrameDisplayFunc)(TFMPVideoFrameBuffer *, void *context);
    typedef int (*AudioFrameDisplayFunc)(void *context);
    
    class DisplayController{
        
        pthread_t dispalyThread;
        static void *displayLoop(void *context);
        
        bool shouldDisplay;
        
    public:
        
        TFMPMediaType displayMediaType = TFMP_MEDIA_TYPE_ALL_AVIABLE;
        
        //the real display func different with different platform
        void *displayContext;
        VideoFrameDisplayFunc displayVideoFrame;
        AudioFrameDisplayFunc displayAudioFrame;
        
        RecycleBuffer<AVFrame*> *shareVideoBuffer;
        RecycleBuffer<AVFrame*> *shareAudioBuffer;
        
        SyncClock *syncClock;

        void startDisplay();
        void stopDisplay();
        
        void freeResource();
    };
}

#endif /* DisplayController_hpp */
