//
//  VideoFormat.h
//  TFMediaPlayer
//
//  Created by shiwei on 17/12/29.
//  Copyright © 2017年 shiwei. All rights reserved.
//

#ifndef VideoFormat_h
#define VideoFormat_h

/** general */

typedef enum{
    TFMP_MEDIA_TYPE_NONE  = 0,
    TFMP_MEDIA_TYPE_VIDEO = 1 << 0,
    TFMP_MEDIA_TYPE_AUDIO = 1 << 1,
    TFMP_MEDIA_TYPE_SUBTITLE = 1 << 2,
    TFMP_MEDIA_TYPE_ALL_AVIABLE = TFMP_MEDIA_TYPE_VIDEO | TFMP_MEDIA_TYPE_AUDIO | TFMP_MEDIA_TYPE_SUBTITLE,
}TFMPMediaType;



/** video */

typedef enum{
    TFMP_VIDEO_PIX_FMT_YUV420P,
    TFMP_VIDEO_PIX_FMT_NV12,
    TFMP_VIDEO_PIX_FMT_NV21,
    TFMP_VIDEO_PIX_FMT_RGB32,
}TFMPVideoPixelFormat;

typedef struct{
    int width;
    int height;
    TFMPVideoPixelFormat format;
    
    int planes;
    
    uint8_t *pixels[8];
    int linesize[8];
    
    bool shouldFreePixels;
    
}TFMPVideoFrameBuffer;

typedef int (*TFMPVideoFrameDisplayFunc)(TFMPVideoFrameBuffer *, void *context);

/** audio */

typedef struct{
    double sampleRate;
    //bit 1 for is int, bit 2 for is signed, bit 3 for is bigEndian, bit 4 for is planar.
    uint8_t formatFlags;
    int bitsPerChannel;
    int channelsPerFrame;
    
    uint64_t ffmpeg_channel_layout;
    
    int bufferSize;
    int samples;
    
}TFMPAudioStreamDescription;

inline void setFormatFlagsWithFlags(uint8_t *formatFlags, bool isInt, bool isSigned, bool isBigEndian, bool isPlanar){
    if (isInt)          *formatFlags |= 1;
    if (isSigned)       *formatFlags |= 1 << 1;
    if (isBigEndian)    *formatFlags |= 1 << 2;
    if (isPlanar)       *formatFlags |= 1 << 3;
}

inline bool isIntForFormatFlags(uint8_t formatFlags){
    return (formatFlags & 1);
}

inline bool isSignedForFormatFlags(uint8_t formatFlags){
    return (formatFlags & (1<<1));
}

inline bool isBigEndianForFormatFlags(uint8_t formatFlags){
    return (formatFlags & (1<<2));
}

inline bool isPlanarForFormatFlags(uint8_t formatFlags){
    return (formatFlags & (1<<3));
}


typedef int (*TFMPFillAudioBufferFunc)(uint8_t **buffer, int lineCount, int oneLineize,void *context);

typedef struct{
    TFMPFillAudioBufferFunc fillFunc; //The system audio player call this function to obtain audio buffer.
    void *context;  //The var that assigned to fillFunc‘s parameter context.
}TFMPFillAudioBufferStruct;




#endif /* VideoFormat_h */
