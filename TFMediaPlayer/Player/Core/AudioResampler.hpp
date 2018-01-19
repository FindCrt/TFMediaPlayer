//
//  AudioResampler.hpp
//  TFMediaPlayer
//
//  Created by shiwei on 18/1/11.
//  Copyright © 2018年 shiwei. All rights reserved.
//

#ifndef AudioResampler_hpp
#define AudioResampler_hpp

#include <stdio.h>
#include "TFMPUtilities.h"
extern "C"{
#include <libswresample/swresample.h>
}

namespace tfmpcore {
    
    //inline bool isNeedResamplexx(AVFrame *sourceFrame, TFMPAudioStreamDescription *destDesc);
    
    class AudioResampler{
        
        
        SwrContext *swrCtx = nullptr;
        void initResampleContext(AVFrame *sourceFrame);
        
        TFMPAudioStreamDescription *lastSourceAudioDesc = nullptr;
        
        uint8_t *resampledBuffers1 = nullptr;
    public:
        
        uint8_t *resampledBuffers = nullptr;
        unsigned int resampleSize = 0;
        
        TFMPAudioStreamDescription adoptedAudioDesc;
        
        bool isNeedResample(AVFrame *sourceFrame);
        
        bool reampleAudioFrame(AVFrame *inFrame, int *outSamples, int *linesize);
        bool reampleAudioFrame2(AVFrame *inFrame, int *outSamples, int *linesize);
        
        void freeResources();
    };
}

#endif /* AudioResampler_hpp */
