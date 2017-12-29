//
//  UnitTest.m
//  TFMediaPlayer
//
//  Created by shiwei on 17/12/29.
//  Copyright © 2017年 shiwei. All rights reserved.
//

extern "C"{
#include <libavutil/time.h>
}

#import "UnitTest.h"
#import "RecycleBuffer.hpp"



@implementation UnitTest

-(void)testRecycleBuffer{
    
    NSLog(@"testRecycleBuffer");

    tfmpcore::RecycleBuffer<int> *buffer = new tfmpcore::RecycleBuffer<int>(50);
    int testCount = 1000;
    __block int outCount = 0;
    __block int inCount = 0;
    
    dispatch_group_t group = dispatch_group_create();

    dispatch_group_enter(group);
    dispatch_async(dispatch_get_global_queue(0, 0), ^{
        
        while (inCount < testCount) {

            if (!buffer->isFull()) {
                int inNum = inCount+1;

                if (buffer->insert(inNum)) {
//                    printf(" <%d> ",inNum);
                    inCount++;
                }
            }
            av_usleep(100);
        }
        
        dispatch_group_leave(group);

    });

    dispatch_group_enter(group);
    dispatch_async(dispatch_get_global_queue(0, 0), ^{
        
        int lastNum = 0;
        while (outCount < testCount) {
            int num;
            if (buffer->getOut(&num)) {
                printf(" [%d] ",num);
                outCount++;
                
                if (lastNum > num) {
                    NSAssert(0, @"order error!");
                }
                lastNum = num;
            }
        }
        
        dispatch_group_leave(group);
    });
    
    dispatch_group_wait(group, DISPATCH_TIME_FOREVER);
    
    NSLog(@"****************\ntest Down: %d, %d",inCount, outCount);
}


@end
