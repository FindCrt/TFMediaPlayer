//
//  SyncClock.hpp
//  TFMediaPlayer
//
//  Created by shiwei on 17/12/29.
//  Copyright © 2017年 shiwei. All rights reserved.
//

#ifndef SyncClock_hpp
#define SyncClock_hpp

#include <stdlib.h>
#include <stdio.h>

namespace tfmpcore {
    class SyncClock{
        //frame pts + correction = the real present time that come from av_gettime_relative.
        double ptsCorrection;
        
        bool isAudioMajor;
        
    public:
        
        SyncClock(bool isAudioMajor = true):isAudioMajor(isAudioMajor){};
        
        
        double lastPts = 0;
        double remainTime(double videoPts, double audioPts);
        
        double nextMediaPts(double videoPts, double audioPts);
        
        void correctWithPresent(double videoPts, double audioPts);
    };
}

#endif /* SyncClock_hpp */
