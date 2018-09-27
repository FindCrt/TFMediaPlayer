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

inline uint64_t channelLayoutForChannels(int channels){
    return av_get_default_channel_layout(channels);
}

#define TFMPCondWait(cond, mutex) \
    pthread_mutex_lock(&mutex);\
    pthread_cond_wait(&cond, &mutex);\
    pthread_mutex_unlock(&mutex);   \

#define TFMPCondSignal(cond, mutex) \
    pthread_mutex_lock(&mutex);\
    pthread_cond_signal(&cond);\
    pthread_mutex_unlock(&mutex);   \


#pragma mark - bytes funcs

/** 提取一个字节中指定区域的数值 */
inline static uint8_t extractbits(uint8_t num, short start, short end){
    if (start == 1) {
        uint8_t result = num;
        result >>= 8-end;
        return result;
    }else{
        uint8_t result = (1<<(9-start))-(1<<(8-end));
        result &= num;
        result >>= 8-end;
        return result;
    }
}

#pragma mark - color space funcs

/**
 yyyy yyyy
 uv    uv
 ->
 yyyy yyyy
 uu
 vv
 */
inline void yuv420sp_to_yuv420p(unsigned char* yuv420sp, unsigned char* yuv420p, int width, int height)
{
    int i, j;
    int y_size = width * height;
    
    unsigned char* y = yuv420sp;
    unsigned char* uv = yuv420sp + y_size;
    
    unsigned char* y_tmp = yuv420p;
    unsigned char* u_tmp = yuv420p + y_size;
    unsigned char* v_tmp = yuv420p + y_size * 5 / 4;
    
    // y
    memcpy(y_tmp, y, y_size);
    
    // u
    for (j = 0, i = 0; j < y_size/2; j+=2, i++)
    {
        u_tmp[i] = uv[j];
        v_tmp[i] = uv[j+1];
    }
}

/**
 yyyy yyyy
 uu
 vv
 ->
 yyyy yyyy
 uv    uv
 */
inline void yuv420p_to_yuv420sp(unsigned char* yuv420p, unsigned char* yuv420sp, int width, int height)
{
    int i, j;
    int y_size = width * height;
    
    unsigned char* y = yuv420p;
    unsigned char* u = yuv420p + y_size;
    unsigned char* v = yuv420p + y_size * 5 / 4;
    
    unsigned char* y_tmp = yuv420sp;
    unsigned char* uv_tmp = yuv420sp + y_size;
    
    // y
    memcpy(y_tmp, y, y_size);
    
    // u
    for (j = 0, i = 0; j < y_size/2; j+=2, i++)
    {
        // 此处可调整U、V的位置，变成NV12或NV21
#if 01
        uv_tmp[j] = u[i];
        uv_tmp[j+1] = v[i];
#else
        uv_tmp[j] = v[i];
        uv_tmp[j+1] = u[i];
#endif
    }
}


#endif /* TFUtilities_h */
