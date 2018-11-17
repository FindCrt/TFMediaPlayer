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
#include "VTBDecoder.h"
#include <pthread.h>
#include <functional>
#include "DisplayController.hpp"
#include "TFMPAVFormat.h"
#include "AudioResampler.hpp"
#include "TFMPDebugFuncs.h"
#include "TFMPFrame.h"

namespace tfmpcore {
    
    typedef int (*FillAudioBufferFunc)(void *buffer, int64_t size, void *context);
    
    class PlayController{
        
        std::string mediaPath;
        
        AVFormatContext *fmtCtx;
        
        int videoStrem = -1;
        int audioStream = -1;
        int subTitleStream = -1;
        
        Decoder *videoDecoder = nullptr;
        Decoder *audioDecoder = nullptr;
        Decoder *subtitleDecoder = nullptr;
        
        DisplayController *displayer = nullptr;
        
        TFMPMediaType desiredDisplayMediaType = TFMP_MEDIA_TYPE_ALL_AVIABLE;
        TFMPMediaType realDisplayMediaType = TFMP_MEDIA_TYPE_NONE;
        void calculateRealDisplayMediaType();

        double duration = 0;
        
        
        /*** A lot of controls and states ***/
        
        //0. prepare
        static int connectFail(void *opque);
        bool abortConnecting = false;
        bool prapareOK = false;
        //real audio format
        void resolveAudioStreamFormat();
        void setupSyncClock();
        
        //1. start
        pthread_t readThread;
        static void * readFrame(void *context);
        
        //2. pause and resume
        bool paused = false;   //It's order from outerside, not state of player. In other word, it's a mark.
        
        //3. stop
        bool abortRequest = false;
        static bool checkEnd(void *context);
        
        //5. seek
        static void seekEnd(void *context);
        /**
         * The state of seeking.
         * It becomes true when the user drags the progressBar and loose fingers.
         * And it becomes false when displayer find the first frame whose pts is greater than the seeking time, because now is time we can actually resume playing.
         */
        bool seeking = false;
        double seekPos = 0;
        
        //6. free
        void resetStatus();

    public:
        
        ~PlayController(){
            if (prapareOK) stop();
        }
        
        void setVideoDecoder(Decoder *decoder){
            videoDecoder = decoder;
        }
        
        void setAudioDecoder(Decoder *decoder){
            audioDecoder = decoder;
        }
        
        /** controls **/
        
        bool connectAndOpenMedia(std::string mediaPath);
        void cancelConnecting();
        
        /** callback which call when playing stoped. The second param is reason code of the reason to stop:
         *  -1 error occur
         *  0 reaching file end
         *  1 stop by calling func stop()
         */
        std::function<void(PlayController*, TFMPStopReason)> playStoped;
        
        void play();
        void pause(bool flag);
        void stop();  //大概率阻塞线程，注意在非主线程调用
        
        bool accurateSeek = true;
        void seekTo(double time);
        void seekByForward(double interval);
        std::function<void(PlayController*)>seekingEndNotify;
        
        std::function<void(PlayController*, bool)> bufferingStateChanged;
        
        /** properties **/
        
        double getDuration();
        double getCurrentTime();
        
        TFMPSyncClockMajor clockMajor = TFMP_SYNC_CLOCK_MAJOR_AUDIO;
        
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
