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
    
    typedef int (*FillAudioBufferFunc)(void *buffer, int64_t size, void *context);
    
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
        
        //read frames
        bool shouldRead = false;
        void startReadingFrames();
        pthread_t readThread;
        static void * readFrame(void *context);
        
        //real audio format
        void resolveAudioStreamFormat();
        
        
        TFMPMediaType desiredDisplayMediaType = TFMP_MEDIA_TYPE_ALL_AVIABLE;
        TFMPMediaType realDisplayMediaType = TFMP_MEDIA_TYPE_NONE;
        
        void calculateRealDisplayMediaType();
        
        void freeResources();
        
    public:
        
        ~PlayController(){
            stop();
            delete displayer;
        }
        
        /** controls **/
        
        bool connectAndOpenMedia(std::string mediaPath);
        std::function<void(PlayController*)> connectCompleted;
        
        void play();
        void pause();
        void stop();
        
        
        /** properties **/
        
        //only work before the call of connectAndOpenMedia.
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
