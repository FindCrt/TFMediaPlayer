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
#include "TFMPAVFormat.hpp"
#include "RecycleBuffer.hpp"
#include "SyncClock.hpp"

extern "C"{
#include <libavformat/avformat.h>
}

namespace tfmpcore {
    
<<<<<<< Updated upstream
=======
    typedef enum{
        TFMP_MEDIA_TYPE_VIDEO = 1 << 0,
        TFMP_MEDIA_TYPE_AUDIO = 1 << 1,
        TFMP_MEDIA_TYPE_SUBTITLE = 1 << 2,
        TFMP_MEDIA_TYPE_ALL_AVIABLE = TFMP_MEDIA_TYPE_VIDEO | TFMP_MEDIA_TYPE_AUDIO | TFMP_MEDIA_TYPE_SUBTITLE,
    }TFMPMediaType;
    
    //the format of audio stream is always PCM, so there isn't variable for format.
    typedef struct{
        double sampleRate;
        char formatFlags;  //bit 1 for is int, bit 2 for is signed, bit 3 for is bigEndian.
        int bitsPerChannel;
        int channelPerFrame;
    }AudioStreamDescription;
    
    typedef int (*VideoFrameDisplayFunc)(TFMPVideoFrameBuffer *, void *context);
    
>>>>>>> Stashed changes
    class DisplayController{
        
        pthread_t dispalyThread;
        static void *displayLoop(void *context);
        
        bool shouldDisplay;
        
    public:
        
        TFMPMediaType displayMediaType = TFMP_MEDIA_TYPE_ALL_AVIABLE;
        
        //the real display func different with different platform
        void *displayContext;
<<<<<<< Updated upstream
        TFMPVideoFrameDisplayFunc displayVideoFrame;
        TFMPFillAudioBufferStruct getFillAudioBufferStruct();
=======
        VideoFrameDisplayFunc displayVideoFrame;
        static int fillAudioBuffer(void *buffer, int64_t size, void *context);
>>>>>>> Stashed changes
        
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
