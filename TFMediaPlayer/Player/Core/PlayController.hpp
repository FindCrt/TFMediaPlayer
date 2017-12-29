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

namespace tfmpcore {
    class PlayController{
        
        std::string mediaPath;
        
        AVFormatContext *fmtCtx;
        AVPacket packet;
        
        int videoStrem;
        int audioStream;
        int subTitleStream;
        
        Decoder *videoDecoder;
        Decoder *audioDecoder;
        Decoder *subtitleDecoder;
        
        DisplayController *displayer = new DisplayController();
        
        bool prapareOK;
        
        void startReadingFrames();
        
        pthread_t readThread;
        
        static void * readFrame(void *context);
        
    public:
        
        /** controls **/
        
        bool connectAndOpenMedia(std::string mediaPath);
        std::function<void(PlayController)> connectCompleted;
        
        void play();
        void pause();
        void stop();
        
        
        /** properties **/
        
        //only work before the call of connectAndOpenMedia.
        bool isAudioMajor = true;
        
        void *displayContext;
        VideoFrameDisplayFunc displayVideoFrame;
        AudioFrameDisplayFunc displayAudioFrame;
        
    };
}

#endif /* PlayController_hpp */
