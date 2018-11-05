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
#include <math.h>
#include <string>

namespace tfmpcore {
    class SyncClock{
        /** 现实时间和媒体时间的差值，代表两个时间流的关系 */
        double realDiff = NAN;
        double mediaTime = 0;
        
    public:
#if DEBUG
        std::string name;
#endif
        int serial = 0;
        bool paused = false;
        
        /** 获取这个同步钟的媒体时间 */
        double getTime();
        
        /** 更新这个同步钟的媒体时间，serial作为seek的标记 */
        void updateTime(double time, int serial, double updateTime = -1);
        
        /** 传入的pts需要多久才可以播 */
        double getRemainTime(double pts);
    };
}

#endif /* SyncClock_hpp */
