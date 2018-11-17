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
#include <libavutil/rational.h>
#include <libavutil/avutil.h>
}

using namespace tfmpcore;

double SyncClock::getTime(){
    if (paused || mediaTime == 0 || realDiff == NAN) {
        return mediaTime;
    }
    
    double realTime = (double)av_gettime_relative()/AV_TIME_BASE;
    return realTime-realDiff;
}

void SyncClock::updateTime(double time, int serial, double realTime){
    
    this->serial = serial;
    mediaTime = time;
    
    if (realTime < 0) {
        realTime = (double)av_gettime_relative()/AV_TIME_BASE;
    }
    realDiff = realTime-mediaTime;
    
    myStateObserver.labelMark(name, to_string(mediaTime));
}

void SyncClock::updateDiff(){
    updateTime(getTime(), serial);
}

double SyncClock::getRemainTime(double pts){
    return pts-getTime();
}
