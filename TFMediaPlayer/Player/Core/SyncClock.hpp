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
        /** 现实时间和媒体时间的差值，代表两个时间流的关系 */
        double realDiff = 0;
        double mediaTime = 0;
        
    public:
        int serial = 0;
        bool paused = false;
        
        double getTime();
        void updateTime(double time, int serial, double updateTime = -1);
        double getDelay(double pts);
    };
}

#endif /* SyncClock_hpp */
