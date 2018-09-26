//
//  TFHardDecodeViewController.m
//  TFMediaPlayer
//
//  Created by shiwei on 2018/9/26.
//  Copyright © 2018年 shiwei. All rights reserved.
//

#import "TFHardDecodeViewController.h"
#import "TFHardDecoder.h"

@interface TFHardDecodeViewController (){
    TFHardDecoder *_hardDecoder;
}

@end

@implementation TFHardDecodeViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    
    NSString *filePath = [[NSBundle mainBundle] pathForResource:@"cocosvideo" ofType:@"mp4"];
    _hardDecoder = [[TFHardDecoder alloc] init];
    _hardDecoder.mediaUrl = [NSURL fileURLWithPath:filePath];
    
    [_hardDecoder test];
}

@end
