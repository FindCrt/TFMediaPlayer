//
//  TFHardDecoder.h
//  TFMediaPlayer
//
//  Created by shiwei on 2018/9/26.
//  Copyright © 2018年 shiwei. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <CoreVideo/CoreVideo.h>

typedef void (^TFHDDecodeFrameHandler)(CVPixelBufferRef pixelBuffer);

@interface TFHardDecoder : NSObject

@property (nonatomic) NSURL *mediaUrl;

@property (nonatomic) TFHDDecodeFrameHandler frameHandler;

-(void)startWithFrameHandler:(TFHDDecodeFrameHandler)frameHandler;

@end
