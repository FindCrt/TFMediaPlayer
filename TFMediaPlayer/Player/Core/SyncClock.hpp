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
#include <libavutil/rational.h>

namespace tfmpcore {
    class SyncClock{
        //frame pts + correction = the real present time that come from av_gettime_relative.
        int64_t ptsCorrection = -1;
        
        double minPtsLimit = 0;
        
    public:
        
        bool isAudioMajor = true;
        
        SyncClock(bool isAudioMajor = true):isAudioMajor(isAudioMajor){};
        
        //unit is microseconds 
        int64_t lastRealPts = 0;
        
        double presentTimeForVideo(int64_t videoPts, AVRational timeBase);
        double presentTimeForAudio(int64_t audioPts, AVRational timeBase);
        
        double remainTimeForVideo(int64_t videoPts, AVRational timeBase);
        double remainTimeForAudio(int64_t audioPts, AVRational timeBase);
        
        void presentVideo(int64_t videoPts, AVRational timeBase);
        void presentAudio(int64_t audioPts, AVRational timeBase, double delay);
        
        void setMinPtsLimit(double minPtsLimit){
            this->minPtsLimit = minPtsLimit;
        }
        
        void reset();
    };
}

#endif /* SyncClock_hpp */
