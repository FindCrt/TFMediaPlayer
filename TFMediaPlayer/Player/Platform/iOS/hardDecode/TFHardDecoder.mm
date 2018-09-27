//
//  TFHardDecoder.m
//  TFMediaPlayer
//
//  Created by shiwei on 2018/9/26.
//  Copyright © 2018年 shiwei. All rights reserved.
//

//https://www.cnblogs.com/yjg2014/p/6144977.html

#import "TFHardDecoder.h"
#include "TFMPUtilities.h"
#import <VideoToolbox/VideoToolbox.h>

extern "C"{
    #import <libavutil/time.h>
}

#define TFHDOutWidth    800
#define TFHDOutHeight   600

@interface TFHardDecoder (){
    dispatch_queue_t _readQueue;
    BOOL _shouldRead;
    NSInputStream *_readStream;
    
    uint8_t *_sps;
    uint8_t *_pps;
    
    size_t _spsSize;
    size_t _ppsSize;
    
    CMVideoFormatDescriptionRef _videoFmtDesc;
    VTDecompressionSessionRef _decodeSession;
}

@end

@implementation TFHardDecoder

inline static bool bytesComp(uint8_t *bytes1, uint8_t *bytes2, int len){
    for (int i = 0; i<len; i++) {
        if (bytes1[i] != bytes2[i]) {
            return false;
        }
    }
    return true;
}

#define resetNalu \
[nalu resetBytesInRange:NSMakeRange(0, nalu.length)];\
nalu.length = 0;\
[nalu appendBytes:startCode length:4];\

-(void)startWithFrameHandler:(TFHDDecodeFrameHandler)frameHandler{
    
    self.frameHandler = frameHandler;
    
    NSMutableData *nalu = [[NSMutableData alloc] init];
    __block int endState = 0;
    __weak typeof(self) weakSelf = self;
    
    const void *startCode = "\x00\x00\x00\x01";
    
    [self startReadWithHandler:^(uint8_t *buffer, NSUInteger validSize) {
        int cur = 0;
        
        uint8_t head1[3] = {0,0,1};
        uint8_t head2[4] = {0,0,0,1};
        if (endState > 0) {
            if (endState < 3 && bytesComp(buffer, head1+endState, 3-endState)) {
                nalu.length -= endState;
                [weakSelf processNALU:nalu];
                resetNalu
                cur += 3-endState;
                
            }else if (bytesComp(buffer, head2+endState, 4-endState)){
                nalu.length -= endState;
                [weakSelf processNALU:nalu];
                resetNalu
                cur += 4-endState;
            }
            
            endState = 0;
        }

        int start = 0;
        while (cur < validSize) {
            if (buffer[cur] == 0) {
                if (cur+1 == validSize) {
                    endState = 1;
                    break;
                }
                if (buffer[cur+1] == 0) {
                    if (cur+2 == validSize) {
                        endState = 2;
                        break;
                    }
                    if (buffer[cur+2] == 1) {
                        [nalu appendBytes:buffer+start length:cur-start];
                        [weakSelf processNALU:nalu];
                        resetNalu
                        
                        start = cur+3;
                        cur = start;
                        continue;
                    }else if (buffer[cur+2] == 0){
                        if (cur+3 == validSize) {
                            endState = 3;
                            break;
                        }
                        if (buffer[cur+3] == 1) {
                            [nalu appendBytes:buffer+start length:cur-start];
                            [weakSelf processNALU:nalu];
                            resetNalu
                            
                            start = cur+4;
                            cur = start;
                            continue;
                        }
                    }
                }
            }
            cur++;
        }
        
        [nalu appendBytes:buffer+start length:validSize-start+1];
    }];
}


-(void)startReadWithHandler:(void(^)(uint8_t *buffer, NSUInteger validSize))handler{
    _shouldRead = YES;
    
    _readStream = [[NSInputStream alloc] initWithURL:_mediaUrl];
    [_readStream open];
    _readQueue = dispatch_queue_create("read queue", DISPATCH_QUEUE_CONCURRENT);
    
    dispatch_async(_readQueue, ^{
        
        NSUInteger size = 1024, validSize = 0;
        uint8_t buffer[size];
        while (_shouldRead) {
            validSize = [_readStream read:buffer maxLength:size];
            if (validSize == 0 || validSize > size) {
                break;
            }
            
            handler(buffer, validSize);
        }
    });
}

-(void)processNALU:(NSMutableData *)nalu{
    if (nalu.length == 0) {
        return;
    }
    
    uint8_t *bytes = (uint8_t*)nalu.bytes;
    
    //第1位为禁止位，为1代表语法错误；2-3为参考级别；4-8为类型
//    uint8_t level = extractbits(bytes[0], 2, 3);
    uint8_t type = extractbits(bytes[4], 4, 8);
    
//    if (type ==0 || (type>=16 && type<=18) || type>20) {  //备用的值
//        return;
//    }
    
    if (type == 7) { //sps
        NSLog(@"sps");
        _spsSize = nalu.length - 4;
        _sps = (uint8_t*)malloc(_spsSize);
        memcpy(_sps, bytes+4, _spsSize);
        [self initDecoder];
    }else if (type == 8){ //pps
        NSLog(@"pps");
        _ppsSize = nalu.length - 4;
        _pps = (uint8_t*)malloc(_ppsSize);
        memcpy(_pps, bytes+4, _ppsSize);
        [self initDecoder];
    }else if (type == 5){
        NSLog(@"i帧");
        if (_decodeSession) {
            uint32_t len = CFSwapInt32BigToHost((uint32_t)nalu.length-4);
            [nalu replaceBytesInRange:NSMakeRange(0, 4) withBytes:&len];
            [self decodeFrame:bytes size:nalu.length];
        }
    }else if (type == 9){
        NSLog(@"************************分界符*************************");
    }else{
        NSLog(@"其他%d",type);
        if (_decodeSession) {
            uint32_t len = CFSwapInt32BigToHost((uint32_t)nalu.length-4);
            [nalu replaceBytesInRange:NSMakeRange(0, 4) withBytes:&len];
            [self decodeFrame:bytes size:nalu.length];
            
            
        }
    }
    
//    printf("\n");
}

-(BOOL)initDecoder{
    
    if (_decodeSession) {
        return YES;
    }
    
    if (_spsSize == 0 || _ppsSize == 0) {
        return NO;
    }
    
    const uint8_t * params[2] = {_sps, _pps};
    const size_t paramSize[2] = {_spsSize, _ppsSize};
    OSStatus status = CMVideoFormatDescriptionCreateFromH264ParameterSets(kCFAllocatorDefault, 2, params, paramSize, 4, &_videoFmtDesc);
    if (status != 0) {
        NSLog(@"create video desc failed!");
        return NO;
    }
    //硬解必须是 kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange
    //                                                           或者是kCVPixelFormatType_420YpCbCr8Planar
    //因为iOS是  nv12  其他是nv21
    NSDictionary *destImageAttris = @{
                                      (id)kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange),
                                      (id)kCVPixelBufferWidthKey : @(TFHDOutHeight),
                                      (id)kCVPixelBufferHeightKey : @(TFHDOutWidth),
                                      //这里款高和编码反的
                                      (id)kCVPixelBufferOpenGLCompatibilityKey : @(YES)
                                      };
    VTDecompressionOutputCallbackRecord callback = {decodeCallback, (__bridge void*)self};
    
    status = VTDecompressionSessionCreate(
                                 kCFAllocatorDefault,
                                 _videoFmtDesc,
                                 NULL,
                                 (__bridge CFDictionaryRef)destImageAttris,
                                 &callback,
                                 &_decodeSession);
    
    return status == 0;
}

-(void)decodeFrame:(uint8_t *)frame size:(size_t)size{
    
    CMBlockBufferRef buffer;
    OSStatus status = CMBlockBufferCreateWithMemoryBlock(NULL, frame, size, kCFAllocatorNull, NULL, 0, size, 0, &buffer);
    if (status) {
        NSLog(@"create block buffer error!");
        return;
    }
    
    CMSampleBufferRef sample;
    const size_t sampleSize[] = {size};
    status = CMSampleBufferCreateReady(kCFAllocatorDefault,
                                       buffer,
                                       _videoFmtDesc,
                                       1,0,NULL, 1, sampleSize, &sample);
    if (status || sample == nil) {
        NSLog(@"create sample buffer error!");
        return;
    }
    
    CVPixelBufferRef pixelBuffer;
    VTDecodeInfoFlags outFlags;
    status = VTDecompressionSessionDecodeFrame(_decodeSession, sample, 0, &pixelBuffer, &outFlags);
    if (status) {
        NSLog(@"decode frame error: %d",status);
    }
}

void decodeCallback(void * CM_NULLABLE decompressionOutputRefCon,void * CM_NULLABLE sourceFrameRefCon,OSStatus status,VTDecodeInfoFlags infoFlags,CM_NULLABLE CVImageBufferRef imageBuffer,CMTime presentationTimeStamp,CMTime presentationDuration ){
    
    TFHardDecoder *decoder = (__bridge TFHardDecoder *)decompressionOutputRefCon;
    decoder.frameHandler(imageBuffer);
    
    av_usleep(1/3.0*100);
}

@end
