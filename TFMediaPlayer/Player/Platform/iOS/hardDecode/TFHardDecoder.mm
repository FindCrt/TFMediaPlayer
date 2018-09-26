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

@interface TFHardDecoder (){
    dispatch_queue_t _readQueue;
    BOOL _shouldRead;
    NSInputStream *_readStream;
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

-(void)test{
    NSMutableData *nalu = [[NSMutableData alloc] init];
    __block int endState = 0;
    __weak typeof(self) weakSelf = self;
    
    [self startReadWithHandler:^(uint8_t *buffer, NSUInteger validSize) {
        int cur = 0;
        
        uint8_t head1[3] = {0,0,1};
        uint8_t head2[4] = {0,0,0,1};
        if (endState > 0) {
            if (endState < 3 && bytesComp(buffer, head1+endState, 3-endState)) {
                nalu.length -= endState;
                [weakSelf processNALU:nalu];
                
                nalu.length = 0;
//                [nalu appendBytes:head1 length:3];
                cur += 3-endState;
                
            }else if (bytesComp(buffer, head2+endState, 4-endState)){
                nalu.length -= endState;
                [weakSelf processNALU:nalu];
                
                nalu.length = 0;
//                [nalu appendBytes:head2 length:4];
                cur += 4-endState;
            }
            
            endState = 0;
        }

        int start = 0;
        while (cur < validSize) {
//            printf("%x ",buffer[cur]);
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
                        [weakSelf processNALU:[nalu copy]];
                        nalu.length = 0;
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
                            [weakSelf processNALU:[nalu copy]];
                            nalu.length = 0;
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

-(void)processNALU:(NSData *)nalu{
    if (nalu.length == 0) {
        return;
    }
    
    uint8_t *bytes = (uint8_t*)nalu.bytes;
    
    //第1位为禁止位，为1代表语法错误；2-3为参考级别；4-8为类型
    uint8_t level = extractbits(bytes[0], 2, 3);
    uint8_t type = extractbits(bytes[0], 4, 8);
    
    if (type ==0 || (type>=16 && type<=18) || type>20) {
//        printf(">>>>>>>>>>> [%d,%d], len %ld\n",type,bytes[0],nalu.length);
        return;
    }
    if (type == 7) { //sps
        NSLog(@"sps");
    }else if (type == 8){ //pps
        NSLog(@"pps");
    }else if (type == 1){
        NSLog(@"p帧");
    }else if (type == 5){
        NSLog(@"i帧");
    }else if (type == 9){
        NSLog(@"************************分界符*************************");
    }else if (type == 10 || type == 11){
        NSLog(@"结束%d",type);
    }else if (type == 2 || type == 3 || type == 4){
        NSLog(@"分片%d",type);
    }else{
        NSLog(@"其他%d",type);
    }
//
//    printf("\n[%d,%d], len %ld ",type,bytes[0],nalu.length);
//    for (int i = 0; i<10; i++) {
//        printf("%x ",((uint8_t*)nalu.bytes)[i]);
//    }
    printf("\n");
}


@end
