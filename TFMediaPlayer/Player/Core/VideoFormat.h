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

#endif /* VideoFormat_h */
