//
//  VideoFormat.h
//  TFMediaPlayer
//
//  Created by shiwei on 17/12/29.
//  Copyright © 2017年 shiwei. All rights reserved.
//

#ifndef VideoFormat_h
#define VideoFormat_h

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
    
    unsigned char *pixels[8];
    unsigned short linesize[8];
    
}TFMPVideoFrameBuffer;

typedef struct{
    double sampleRate;
    char formatFlags;  //bit 1 for is int, bit 2 for is signed, bit 3 for is bigEndian.
    int bitsPerChannel;
    int channelPerFrame;
}TFMPAudioStreamDescription;

typedef enum{
    TFMP_MEDIA_TYPE_VIDEO = 1 << 0,
    TFMP_MEDIA_TYPE_AUDIO = 1 << 1,
    TFMP_MEDIA_TYPE_SUBTITLE = 1 << 2,
    TFMP_MEDIA_TYPE_ALL_AVIABLE = TFMP_MEDIA_TYPE_VIDEO | TFMP_MEDIA_TYPE_AUDIO | TFMP_MEDIA_TYPE_SUBTITLE,
}TFMPMediaType;

typedef int (*TFMPVideoFrameDisplayFunc)(TFMPVideoFrameBuffer *, void *context);

typedef int (*TFMPFillAudioBufferFunc)(void *buffer, int size,void *context);

typedef struct{
    TFMPFillAudioBufferFunc fillFunc; //The system audio player call this function to obtain audio buffer.
    void *context;  //The var that assigned to fillFunc‘s parameter context.
}TFMPFillAudioBufferStruct;


#endif /* VideoFormat_h */
