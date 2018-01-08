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

double SyncClock::remainTime(double videoPts, double audioPts){
    
    double currentTime = av_gettime_relative() / 1000000.0;
    
    if (ptsCorrection == 0) {
        return 0.01;
    }
    
    if (isAudioMajor) {
        
        return audioPts + ptsCorrection - currentTime;
        
    }else{
        
        return videoPts - lastPts;
        
        //return videoPts + ptsCorrection - currentTime;
    }
}

double SyncClock::nextMediaPts(double videoPts, double audioPts){
    
    if (isAudioMajor) {
        return videoPts;
    }else{
        return audioPts;
    }
}

void SyncClock::correctWithPresent(double videoPts, double audioPts){
    
    double currentTime = av_gettime_relative() / 1000000.0;
    
    if (isAudioMajor) {
        lastPts = audioPts;
        ptsCorrection = currentTime - audioPts;
    }else{
        lastPts = videoPts;
        ptsCorrection = currentTime - videoPts;
    }
    
    
    printf("ptsCorrection: %lld\n",ptsCorrection);
}
