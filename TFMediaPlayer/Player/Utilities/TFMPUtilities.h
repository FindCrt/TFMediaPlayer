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
    
    if (audioFormat == AV_SAMPLE_FMT_NONE ||
        audioFormat == AV_SAMPLE_FMT_NB) {
        return 0;
    }
    
    bool isBigEndian = BIG_ENDIAN == BYTE_ORDER; //format's data is always in native-endian order.
    bool isInt = false, isSigned = false, isPlanar = false;
    
    if (audioFormat == AV_SAMPLE_FMT_U8 ||
        audioFormat == AV_SAMPLE_FMT_U8P) {
        isSigned = false;
    }else{
        isSigned = true;
    }
    
    if (audioFormat == AV_SAMPLE_FMT_FLT ||
        audioFormat == AV_SAMPLE_FMT_DBL ||
        audioFormat == AV_SAMPLE_FMT_FLTP ||
        audioFormat == AV_SAMPLE_FMT_DBLP ) {
        isInt = false;
    }else{
        isInt = true;
    }
    
    if (audioFormat == AV_SAMPLE_FMT_U8P ||
        audioFormat == AV_SAMPLE_FMT_S16P ||
        audioFormat == AV_SAMPLE_FMT_S32P ||
        audioFormat == AV_SAMPLE_FMT_FLTP ||
        audioFormat == AV_SAMPLE_FMT_DBLP ||
        audioFormat == AV_SAMPLE_FMT_S64P) {
        isPlanar = true;
    }else{
        isPlanar = false;
    }
    
    uint8_t formatFlags = 0;
    
    setFormatFlagsWithFlags(&formatFlags, isInt, isSigned, isBigEndian, isPlanar);
    
    return formatFlags;
}

inline int bitPerChannelFromFFmpegAudioFormat(AVSampleFormat audioFormat){
    if (audioFormat == AV_SAMPLE_FMT_U8 || audioFormat == AV_SAMPLE_FMT_U8P) {
        return 8;
    }else if (audioFormat == AV_SAMPLE_FMT_S16 || audioFormat == AV_SAMPLE_FMT_S16P){
        return 16;
    }else if (audioFormat == AV_SAMPLE_FMT_S32 || audioFormat == AV_SAMPLE_FMT_S32P){
        return 32;
    }else if (audioFormat == AV_SAMPLE_FMT_S64 || audioFormat == AV_SAMPLE_FMT_S64P){
        return 64;
    }else if (audioFormat == AV_SAMPLE_FMT_FLT || audioFormat == AV_SAMPLE_FMT_FLTP){
        return sizeof(float)*8;
    }else if (audioFormat == AV_SAMPLE_FMT_DBL || audioFormat == AV_SAMPLE_FMT_DBLP){
        return sizeof(double)*8;
    }
    
    return 0;
}

inline AVSampleFormat FFmpegAudioFormatFromTFMPAudioDesc(uint8_t formatFlags, int bitPerChannel){
    
    bool isPlanar = isPlanarForFormatFlags(formatFlags);
    
    if (bitPerChannel == 8) {
        return isPlanar ? AV_SAMPLE_FMT_U8P : AV_SAMPLE_FMT_U8;
    }else if (isIntForFormatFlags(formatFlags)){
        if (bitPerChannel == 16) {
            return isPlanar ? AV_SAMPLE_FMT_S16P : AV_SAMPLE_FMT_S16;
        }else if (bitPerChannel == 32){
            return isPlanar ? AV_SAMPLE_FMT_S32P : AV_SAMPLE_FMT_S32;
        }else if (bitPerChannel == 64){
            return isPlanar ? AV_SAMPLE_FMT_S64P : AV_SAMPLE_FMT_S64;
        }
    }else{
        if (bitPerChannel == sizeof(float)*8) {
            return isPlanar ? AV_SAMPLE_FMT_FLTP : AV_SAMPLE_FMT_FLT;
        }else if (bitPerChannel == sizeof(double)*8){
            return isPlanar ? AV_SAMPLE_FMT_DBLP : AV_SAMPLE_FMT_DBL;
        }
    }
    
    return AV_SAMPLE_FMT_NONE;
}


#endif /* TFUtilities_h */
