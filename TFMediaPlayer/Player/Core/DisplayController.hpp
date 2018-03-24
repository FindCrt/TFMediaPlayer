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
#include "AudioResampler.hpp"

extern "C"{
#include <libavformat/avformat.h>
#include <libavutil/time.h>
}

#define TFMP_MAX_AUDIO_CHANNEL 8

namespace tfmpcore {
    
    typedef struct{
        uint32_t allocSize = 0;
        uint32_t validSize = 0;
        uint32_t readIndex = 0;
        
        uint8_t *head = nullptr;
        
        inline uint8_t *readingPoint(){return head+readIndex;}
        inline uint32_t unreadSize(){return validSize-readIndex;};
        
    }TFMPRemainingBuffer;
    
    class DisplayController{
        
        pthread_t dispalyThread;
        static void *displayLoop(void *context);
        
        bool shouldDisplay = false;
        bool paused = false;
        bool isDispalyingVideo = false;
        bool isFillingAudio = false;
        
        TFMPRemainingBuffer remainingAudioBuffers;
        
        static int fillAudioBuffer(uint8_t **buffer, int lineCount,int oneLineSize, void *context);
        
        AudioResampler *audioResampler = nullptr;
        
        uint64_t lastPRealTime = 0;
        int64_t lastPts = 0;
        bool lastIsAudio = true;
        
    public:
        
        ~DisplayController(){
            freeResources();
            delete audioResampler;
            delete syncClock;
        }
        
        TFMPMediaType displayMediaType = TFMP_MEDIA_TYPE_ALL_AVIABLE;
        
        void *displayContext;
        
        //the real display function different with different platform
        TFMPVideoFrameDisplayFunc displayVideoFrame;
        TFMPFillAudioBufferStruct getFillAudioBufferStruct();
        
        void setAudioResampler(AudioResampler *audioResampler){
            this->audioResampler = audioResampler;
        }
        
        RecycleBuffer<AVFrame*> *shareVideoBuffer;
        RecycleBuffer<AVFrame*> *shareAudioBuffer;
        
        AVRational videoTimeBase;
        AVRational audioTimeBase;
        
        
        SyncClock *syncClock = nullptr;
        double getLastPlayTime();
        void setMinPtsLimit(double minPtsLimit){
            if (syncClock) syncClock->setMinPtsLimit(minPtsLimit);
        }

        //controls
        void startDisplay();
        void stopDisplay();
        
        void pause(bool flag);
        
        //release
        void flush();
        void freeResources();
    };
}

#endif /* DisplayController_hpp */
