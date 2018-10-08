//
//  TFMPFrame.h
//  TFMediaPlayer
//
//  Created by shiwei on 2018/9/30.
//  Copyright © 2018年 shiwei. All rights reserved.
//

#ifndef TFMPFrame_h
#define TFMPFrame_h

extern "C"{
#include <libavformat/avformat.h>
}
#include "TFMPAVFormat.h"

namespace tfmpcore {
    
    typedef enum{
        TFMPFrameTypeAudio,
        TFMPFrameTypeVideo,
        TFMPFrameTypeVTBVideo,
    }TFMPFrameType;
    
    class TFMPFrame{
        
    public:
        AVFrame *frame;
        TFMPFrameType type;
        
        uint64_t pts;
        
        void *opaque;
        void (*freeFrameFunc)(TFMPFrame **frame);
        
//        TFMPVideoFrameBuffer *(*convertToDisplayBuffer)(TFMPFrame *frame);
        TFMPVideoFrameBuffer *displayBuffer;
    };
}


#endif /* TFMPFrame_h */
