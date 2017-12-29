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

int64_t SyncClock::remainTime(int64_t videoPts, int64_t audioPts){
    
    int64_t currentTime = av_gettime_relative();
    
    if (isAudioMajor) {
        
        return videoPts + ptsCorrection - currentTime;
        
    }else{
        return audioPts + ptsCorrection - currentTime;
    }
}

int64_t SyncClock::nextMediaPts(int64_t videoPts, int64_t audioPts){
    
    if (isAudioMajor) {
        return videoPts;
    }else{
        return audioPts;
    }
}

void SyncClock::correctWithPresent(int64_t videoPts, int64_t audioPts){
    
    uint64_t currentTime = av_gettime_relative();
    
    if (isAudioMajor) {
        ptsCorrection = currentTime - videoPts;
    }else{
        ptsCorrection = currentTime - videoPts;
    }
}
