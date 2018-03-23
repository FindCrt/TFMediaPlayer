//
//  SyncClock.cpp
//  TFMediaPlayer
//
//  Created by shiwei on 17/12/29.
//  Copyright © 2017年 shiwei. All rights reserved.
//

#include "SyncClock.hpp"
#include "TFMPDebugFuncs.h"

extern "C"{
#include <libavutil/time.h>
}

using namespace tfmpcore;

static double timeDen = 1000000;

void SyncClock::reset(){
    TFMPDLOG_C("reset sync\n");
    ptsCorrection = -1;
}

double SyncClock::presentTimeForVideo(int64_t videoPts, AVRational timeBase){
    if (ptsCorrection < 0) {
        TFMPDLOG_C("ptsCorrection < 0 \n");
        return av_gettime_relative()/timeDen;
    }
    
    int64_t sourcePts = videoPts *av_q2d(timeBase)*timeDen;
    TFMPDLOG_C("sync: %.6f, %.6f\n",ptsCorrection/timeDen, sourcePts/timeDen);
    return (ptsCorrection+sourcePts - lastRealPts)/timeDen;
}

double SyncClock::presentTimeForAudio(int64_t audioPts, AVRational timeBase){
    
    if (ptsCorrection < 0) {
        return av_gettime_relative()/timeDen;
    }
    
    int64_t sourcePts = audioPts *av_q2d(timeBase)*timeDen;
    
    return (ptsCorrection+sourcePts - lastRealPts)/timeDen;
}

//TODO: remain time is much bigger than the duration of frame, discard it and correct ptsCorrection's value.
double SyncClock::remainTimeForVideo(int64_t videoPts, AVRational timeBase){
    
    return presentTimeForVideo(videoPts, timeBase) - av_gettime_relative()/timeDen;
}

double SyncClock::remainTimeForAudio(int64_t audioPts, AVRational timeBase){
    return presentTimeForVideo(audioPts, timeBase) - av_gettime_relative()/timeDen;
}

void SyncClock::presentVideo(int64_t videoPts, AVRational timeBase){
    
    TFMPDLOG_C("presentVideo: %.3f, %lld\n",ptsCorrection/timeDen,videoPts);
    
    if (isAudioMajor) {
        return;
    }
    
    ptsCorrection = av_gettime_relative() - videoPts*av_q2d(timeBase)*timeDen;
}

void SyncClock::presentAudio(int64_t audioPts, AVRational timeBase, double delay){
    if (!isAudioMajor) {
        return;
    }
    
    TFMPDLOG_C("presentAudio: %.3f, %lld\n",ptsCorrection/timeDen,audioPts);
    ptsCorrection = av_gettime_relative() + delay - audioPts*av_q2d(timeBase)*timeDen;
}
