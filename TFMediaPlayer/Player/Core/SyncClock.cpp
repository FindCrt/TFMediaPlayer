//
//  SyncClock.cpp
//  TFMediaPlayer
//
//  Created by shiwei on 17/12/29.
//  Copyright © 2017年 shiwei. All rights reserved.
//

#include "SyncClock.hpp"

extern "C"{
#include <libavutil/time.h>
}

using namespace tfmpcore;

static double timeDen = 1000000;

void SyncClock::reset(){
    ptsCorrection = -1;
}

double SyncClock::presentTimeForVideo(int64_t videoPts, AVRational timeBase){
    if (ptsCorrection < 0) {
        return av_gettime_relative()/timeDen;
    }
    
    int64_t sourcePts = videoPts *av_q2d(timeBase)*timeDen;
    
    return (ptsCorrection+sourcePts - lastRealPts)/timeDen;
}

double SyncClock::presentTimeForAudio(int64_t audioPts, AVRational timeBase){
    
    if (ptsCorrection < 0) {
        return av_gettime_relative()/timeDen;
    }
    
    int64_t sourcePts = audioPts *av_q2d(timeBase)*timeDen;
    
    return (ptsCorrection+sourcePts - lastRealPts)/timeDen;
}

double SyncClock::remainTimeForVideo(int64_t videoPts, AVRational timeBase){
    
    return presentTimeForVideo(videoPts, timeBase) - av_gettime_relative()/timeDen;
}

double SyncClock::remainTimeForAudio(int64_t audioPts, AVRational timeBase){
    return presentTimeForVideo(audioPts, timeBase) - av_gettime_relative()/timeDen;
}

void SyncClock::presentVideo(int64_t videoPts, AVRational timeBase){
    if (isAudioMajor) {
        return;
    }
    
    ptsCorrection = av_gettime_relative() - videoPts*av_q2d(timeBase)*timeDen;
}

void SyncClock::presentAudio(int64_t audioPts, AVRational timeBase, double delay){
    if (!isAudioMajor) {
        return;
    }
    
    ptsCorrection = av_gettime_relative() + delay - audioPts*av_q2d(timeBase)*timeDen;
}
