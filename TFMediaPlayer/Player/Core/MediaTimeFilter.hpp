//
//  MediaTimeFilter.hpp
//  TFMediaPlayer
//
//  Created by shiwei on 18/3/30.
//  Copyright © 2018年 shiwei. All rights reserved.
//

#ifndef MediaTimeFilter_hpp
#define MediaTimeFilter_hpp

#include <stdio.h>
extern "C"{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/time.h>
}

namespace tfmpcore {
    class MediaTimeFilter{
        
    public:
        
        MediaTimeFilter(AVRational timeBase):timeBase(timeBase){};
        bool enable = false;
        
        AVRational timeBase;
        
        double minMediaTime = 0;
        
        inline bool checkFrame(AVFrame *frame, bool isVideo){
            if (!enable) return true;
            
            return (frame->pts * av_q2d(timeBase)) > minMediaTime;
        };
    };
}

#endif /* MediaTimeFilter_hpp */
