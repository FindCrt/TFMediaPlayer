//
//  PlayController.hpp
//  TFMediaPlayer
//
//  Created by shiwei on 17/12/25.
//  Copyright © 2017年 shiwei. All rights reserved.
//

#ifndef PlayController_hpp
#define PlayController_hpp

extern "C"{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

#include <stdio.h>
#include <string>
#include "Decoder.hpp"
#include <pthread.h>
#include <functional>
#include "DisplayController.hpp"
#include "TFMPAVFormat.h"
#include "AudioResampler.hpp"

namespace tfmpcore {
    
    bool videoFrameSizeNotified(RecycleBuffer<AVFrame *> *buffer, int curSize, bool isGreater,void *observer);
    typedef int (*FillAudioBufferFunc)(void *buffer, int64_t size, void *context);
    
    static int playResumeSize = 20;
    static int bufferEmptySize = 1;
    
    class PlayController{
        
        std::string mediaPath;
        
        AVFormatContext *fmtCtx;
        AVPacket packet;
        
        int videoStrem = -1;
        int audioStream = -1;
        int subTitleStream = -1;
        
        Decoder *videoDecoder = nullptr;
        Decoder *audioDecoder = nullptr;
        Decoder *subtitleDecoder = nullptr;
        
        DisplayController *displayer = nullptr;
        
        bool prapareOK;
        
        double duration;
        
        //read frames
        bool shouldRead = false;
        
        void startReadingFrames();
        pthread_t readThread;
        static void * readFrame(void *context);
        
        //real audio format
        void resolveAudioStreamFormat();
        
        /**
         * The state of seeking.
         * It becomes true when the user drags the progressBar and loose fingers.
         * And it becomes false when displayer find the first frame whose pts is greater than the seeking time, because now is time we can actually resume playing.
         */
        bool seeking = false;
        double seekingTime = 0;
        void seekingEnd();
        
        TFMPMediaType desiredDisplayMediaType = TFMP_MEDIA_TYPE_ALL_AVIABLE;
        TFMPMediaType realDisplayMediaType = TFMP_MEDIA_TYPE_NONE;
        
        void calculateRealDisplayMediaType();
        void setupSyncClock();
        
        void freeResources();
        
        bool paused = false;
        pthread_cond_t pause_cond = PTHREAD_COND_INITIALIZER;
        pthread_mutex_t pause_mutex = PTHREAD_MUTEX_INITIALIZER;
        void startCheckPlayFinish();
        
        //catch play ending
        bool checkingEnd = false;
        pthread_t signalThread;
        static void *signalPlayFinished(void *context);
        
    public:
        
        ~PlayController(){
            stop();
            delete displayer;
        }
        
        friend void frameEmpty(Decoder *decoder, void* context);
        
        friend bool tfmpcore::videoFrameSizeNotified(RecycleBuffer<AVFrame *> *buffer, int curSize, bool isGreater,void *observer);
        
        /** controls **/
        
        bool connectAndOpenMedia(std::string mediaPath);
        std::function<void(PlayController*)> connectCompleted;
        
        /** callback which call when playing stoped. The second param is error code of the reason to stop. It's 0 when reason is reaching end of file.*/
        std::function<void(PlayController*, int)> playStoped;
        
        void play();
        void pause(bool flag);
        void stop();
        
        void seekTo(double time);
        void seekByForward(double interval);
        std::function<void(PlayController*)>seekingEndNotify;
        
        /** properties **/
        
        double getDuration();
        double getCurrentTime();
        
        //the real value is affect by realDisplayMediaType. For example, there is no audio stream, isAudioMajor couldn't be true.
        bool isAudioMajor = true;
        
        void setDesiredDisplayMediaType(TFMPMediaType desiredDisplayMediaType);
        TFMPMediaType getRealDisplayMediaType(){
            return realDisplayMediaType;
        }
        
        
        void *displayContext = nullptr;
        TFMPVideoFrameDisplayFunc displayVideoFrame = nullptr;

        TFMPFillAudioBufferStruct getFillAudioBufferStruct();
        
        DisplayController *getDisplayer();
        /** The source part inputs source audio stream desc, the platform-special part return a audio stream desc that will be fine for both parts. */
        std::function<TFMPAudioStreamDescription(TFMPAudioStreamDescription)> negotiateAdoptedPlayAudioDesc;
        FillAudioBufferFunc getFillAudioBufferFunc();
    };
}



#endif /* PlayController_hpp */
