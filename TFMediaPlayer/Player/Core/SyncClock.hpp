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

namespace tfmpcore {
    class SyncClock{
        //frame pts + correction = the real present time that come from av_gettime_relative.
        int64_t ptsCorrection;
        
        bool isAudioMajor;
        
    public:
        
        SyncClock(bool isAudioMajor = true):isAudioMajor(isAudioMajor){};
        
        
        int64_t lastPts;
        int64_t remainTime(int64_t videoPts, int64_t audioPts);
        
        int64_t nextMediaPts(int64_t videoPts, int64_t audioPts);
        
        void correctWithPresent(int64_t videoPts, int64_t audioPts);
    };
}

#endif /* SyncClock_hpp */
