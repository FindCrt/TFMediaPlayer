//
//  TFUtilities.h
//  TFMediaPlayer
//
//  Created by shiwei on 18/1/10.
//  Copyright © 2018年 shiwei. All rights reserved.
//

#ifndef TFUtilities_h
#define TFUtilities_h

extern "C"{
#include <libavformat/avformat.h>
}

#include "TFMPAVFormat.h"

inline uint8_t formatFlagsFromFFmpegAudioFormat(AVSampleFormat audioFormat){
    
    bool isBigEndian = BIG_ENDIAN == BYTE_ORDER; //format's data is always in native-endian order.
    bool isInt = false, isSigned = false;
    
    if (audioFormat == AV_SAMPLE_FMT_U8) {
        isInt = true;
        isSigned = false;
    }else if (audioFormat == AV_SAMPLE_FMT_S16){
        isInt = true;
        isSigned = true;
    }else if (audioFormat == AV_SAMPLE_FMT_S32){
        isInt = true;
        isSigned = true;
    }else if (audioFormat == AV_SAMPLE_FMT_S64){
        isInt = true;
        isSigned = true;
    }else if (audioFormat == AV_SAMPLE_FMT_FLT){
        isInt = false;
        isSigned = true;
    }else if (audioFormat == AV_SAMPLE_FMT_DBL){
        isInt = false;
        isSigned = true;
    }
    
    uint8_t formatFlags = 0;
    
    if (isInt)          formatFlags |= 1;
    if (isSigned)       formatFlags |= 1 << 1;
    if (isBigEndian)    formatFlags |= 1 << 2;
    
    return formatFlags;
}

inline int bitPerChannelFromFFmpegAudioFormat(AVSampleFormat audioFormat){
    if (audioFormat == AV_SAMPLE_FMT_U8) {
        return 8;
    }else if (audioFormat == AV_SAMPLE_FMT_S16){
        return 16;
    }else if (audioFormat == AV_SAMPLE_FMT_S32){
        return 32;
    }else if (audioFormat == AV_SAMPLE_FMT_S64){
        return 64;
    }else if (audioFormat == AV_SAMPLE_FMT_FLT){
        return sizeof(float)*8;
    }else if (audioFormat == AV_SAMPLE_FMT_DBL){
        return sizeof(double)*8;
    }
    return 0;
}

inline AVSampleFormat FFmpegAudioFormatFromTFMPAudioDesc(uint8_t formatFlags, int bitPerChannel){
    if (bitPerChannel == 8) {
        return AV_SAMPLE_FMT_U8;
    }else if (isIntForAudioDesc(formatFlags)){
        if (bitPerChannel == 16) {
            return AV_SAMPLE_FMT_S16;
        }else if (bitPerChannel == 32){
            return AV_SAMPLE_FMT_S32;
        }else if (bitPerChannel == 64){
            return AV_SAMPLE_FMT_S64;
        }
    }else{
        if (bitPerChannel == sizeof(float)*8) {
            return AV_SAMPLE_FMT_FLT;
        }else if (bitPerChannel == sizeof(double)*8){
            return AV_SAMPLE_FMT_DBL;
        }
    }
    
    return AV_SAMPLE_FMT_NONE;
}


#endif /* TFUtilities_h */
