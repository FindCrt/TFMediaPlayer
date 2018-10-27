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
        double realDiff = 0;
        double mediaTime = 0;
        
    public:
        int serial = 0;
        bool paused = false;
        
        double getTime();
        void updateTime(double time, int serial);
        double getDelay(double pts);
        
        void reset();
    };
}

#endif /* SyncClock_hpp */
