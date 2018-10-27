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

void SyncClock::reset(){
    
}

double SyncClock::getTime(){
    if (paused) {
        return mediaTime;
    }
    
    double realTime = av_gettime_relative()/AV_TIME_BASE;
    return realTime-realDiff;
}

void SyncClock::updateTime(double time, int serial){
    
    this->serial = serial;
    mediaTime = time;
    
    double realTime = av_gettime_relative()/AV_TIME_BASE;
    realDiff = realTime-mediaTime;
}

double SyncClock::getDelay(double pts){
    return getTime()
}
