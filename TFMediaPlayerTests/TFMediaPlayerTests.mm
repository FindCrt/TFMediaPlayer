//
//  TFMediaPlayerTests.m
//  TFMediaPlayerTests
//
//  Created by shiwei on 17/12/29.
//  Copyright © 2017年 shiwei. All rights reserved.
//

#import <XCTest/XCTest.h>
#include "RecycleBuffer.hpp"

@interface TFMediaPlayerTests : XCTestCase

@end

@implementation TFMediaPlayerTests

- (void)setUp {
    [super setUp];
    // Put setup code here. This method is called before the invocation of each test method in the class.
}

- (void)tearDown {
    // Put teardown code here. This method is called after the invocation of each test method in the class.
    [super tearDown];
}

-(void)testRecycleBuffer{
    
//    NSLog(@"testRecycleBuffer");
//    
//    tfmpcore::RecycleBuffer<int> *buffer = new tfmpcore::RecycleBuffer<int>(50);
//    int testCount = 1000;
//    int outCount = 0;
//    
//    dispatch_async(dispatch_get_global_queue(0, 0), ^{
//        
//        int inCount = 0;
//        while (inCount < testCount) {
//            
//            if (!buffer->isFull()) {
//                int inNum = arc4random() % 1000000;
//                
//                if (buffer->insert(inNum)) {
//                    printf(" <%d> ",inNum);
//                    inCount++;
//                }
//            }
//        }
//        
//    });
//    
//    while (outCount < testCount) {
//        int num;
//        if (buffer->read(&num)) {
//            printf(" *%d* ",num);
//            outCount++;
//        }
//    }
    
    NSLog(@"test Down");
}

@end
