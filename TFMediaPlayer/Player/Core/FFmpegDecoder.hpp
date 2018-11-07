//
//  FFmpegDecoder.hpp
//  TFMediaPlayer
//
//  Created by shiwei on 2018/11/5.
//  Copyright © 2018年 shiwei. All rights reserved.
//

#ifndef FFmpegDecoder_hpp
#define FFmpegDecoder_hpp

#include <stdio.h>
#include "Decoder.hpp"

namespace tfmpcore{
    class FFmpegDecoder : public Decoder{
        
        static TFMPVideoFrameBuffer *displayBufferFromFrame(TFMPFrame *tfmpFrame);
        static TFMPFrame *tfmpFrameFromAVFrame(AVFrame *frame, bool isAudio, int serial);
        
        inline static void freeFrame(TFMPFrame **tfmpFrameP){
            TFMPFrame *tfmpFrame = *tfmpFrameP;
            av_frame_free(&(tfmpFrame->frame));
            
            delete tfmpFrame->displayBuffer;
            
            delete tfmpFrame;
            *tfmpFrameP = nullptr;
        }
        
    public:
        FFmpegDecoder(){
            decodeLoopFunc = decodeLoop;
        }
        virtual bool prepareDecode();
        static void *decodeLoop(void *context);
    };
}

#endif /* FFmpegDecoder_hpp */
