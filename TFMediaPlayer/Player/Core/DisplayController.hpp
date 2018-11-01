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
#include <functional>
#include <semaphore.h>
#include "TFMPDebugFuncs.h"
#include "VTBDecoder.h"
#include "TFMPFrame.h"

extern "C"{
#include <libavformat/avformat.h>
#include <libavutil/time.h>
}

#define TFMP_MAX_AUDIO_CHANNEL 8

namespace tfmpcore {
    
    static double invalidPlayTime = -1;
    
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
        pthread_cond_t video_pause_cond = PTHREAD_COND_INITIALIZER;
        pthread_mutex_t video_pause_mutex = PTHREAD_MUTEX_INITIALIZER;
        
        TFMPRemainingBuffer remainingAudioBuffers;
        
        static int fillAudioBuffer(uint8_t **buffer, int lineCount,int oneLineSize, void *context);
        
        AudioResampler audioResampler;
        
    public:
        
        /*** display ***/
        
        TFMPMediaType displayMediaType = TFMP_MEDIA_TYPE_ALL_AVIABLE;
        
        void *displayContext = nullptr;
        //video display func
        TFMPVideoFrameDisplayFunc displayVideoFrame;
        //audio display func
        TFMPFillAudioBufferStruct getFillAudioBufferStruct();
        
        //audio output format
        void setAudioDesc(TFMPAudioStreamDescription audioDesc){
            audioResampler.adoptedAudioDesc = audioDesc;
        }
        //decoded media buffers
        RecycleBuffer<TFMPFrame*> *shareVideoBuffer;
        RecycleBuffer<TFMPFrame*> *shareAudioBuffer;
        
        
        /*** sync and play time ***/
        
        int serial = 0;
        
        AVRational videoTimeBase;
        AVRational audioTimeBase;
        double averageAudioDu;
        double averageVideoDu;
        double playerBufferDelay = 0;  //上层播放器的缓冲区数据播放延迟
        double filterTime = 0;      //准确的seek需要时间过滤
        
        //sync clock
        TFMPSyncClockMajor clockMajor = TFMP_SYNC_CLOCK_MAJOR_AUDIO;
        SyncClock *videoClock = new SyncClock();
        SyncClock *audioClock = new SyncClock();
        SyncClock *getMajorClock(){
            if (clockMajor == TFMP_SYNC_CLOCK_MAJOR_AUDIO) {
                return audioClock;
            }else if (clockMajor == TFMP_SYNC_CLOCK_MAJOR_VIDEO){
                return videoClock;
            }else{
                return nil;
            }
        };
        double getPlayTime();

        //controls
        void startDisplay();
        void stopDisplay();
        
        void pause(bool flag);
        bool isPaused(){ return paused;};
    };
}

#endif /* DisplayController_hpp */
