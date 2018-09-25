//
//  SyncClock.cpp
//  TFMediaPlayer
//
//  Created by shiwei on 17/12/29.
//  Copyright © 2017年 shiwei. All rights reserved.
//

#include "SyncClock.hpp"
#include "TFMPDebugFuncs.h"
#include "TFStateObserver.hpp"

extern "C"{
#include <libavutil/time.h>
}

using namespace tfmpcore;

static double timeDen = 1000000;

void SyncClock::reset(){
    myStateObserver.mark("ptsCorrection", -1);
    ptsCorrection = -1;
}

double SyncClock::presentTimeForVideo(int64_t videoPts, AVRational timeBase){
    
    double sourcePts = videoPts *av_q2d(timeBase);
    
    TFMPDLOG_C("video sourcePts: %.3f, minMediaTime: %.3f\n",sourcePts,minMediaTime);
    if (sourcePts < minMediaTime) {
        return 0;  //discard this frame
    }
    
    if (ptsCorrection < 0) {
        TFMPDLOG_C("ptsCorrection < 0 \n");
        return av_gettime_relative()/timeDen;
    }
    
    TFMPDLOG_C("sync: %.6f, %.6f\n",ptsCorrection/timeDen, sourcePts);
    return ptsCorrection/timeDen+sourcePts;
}

double SyncClock::presentTimeForAudio(int64_t audioPts, AVRational timeBase){
    
    double sourcePts = audioPts *av_q2d(timeBase);
    
    TFMPDLOG_C("audio sourcePts: %.3f, minPtsLimit: %.3f\n",sourcePts,minMediaTime);
    if (sourcePts < minMediaTime) {
        return 0; //discard this frame
    }
    
    if (ptsCorrection < 0) {
        myStateObserver.labelMark("sync audio", to_string(sourcePts));
        myStateObserver.timeMark("sync audio dis");
        return av_gettime_relative()/timeDen;
    }
    return ptsCorrection/timeDen+sourcePts;
}

//TODO: remain time is much bigger than the duration of frame, discard it and correct ptsCorrection's value.
double SyncClock::remainTimeForVideo(int64_t videoPts, AVRational timeBase){
    
    return presentTimeForVideo(videoPts, timeBase) - av_gettime_relative()/timeDen;
}

double SyncClock::remainTimeForAudio(int64_t audioPts, AVRational timeBase){
    return presentTimeForAudio(audioPts, timeBase) - av_gettime_relative()/timeDen;
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
    myStateObserver.mark("ptsCorrection", ptsCorrection);
}
