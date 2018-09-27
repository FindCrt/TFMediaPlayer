//
//  TFHardDecodeViewController.m
//  TFMediaPlayer
//
//  Created by shiwei on 2018/9/26.
//  Copyright © 2018年 shiwei. All rights reserved.
//

#import "TFHardDecodeViewController.h"
#import "TFHardDecoder.h"
#import "TFOPGLESDisplayView.h"

@interface TFHardDecodeViewController (){
    TFHardDecoder *_hardDecoder;
}

@property (nonatomic) TFOPGLESDisplayView *displayView;

@end

@implementation TFHardDecodeViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    
    _displayView = [[TFOPGLESDisplayView alloc] initWithFrame:self.view.bounds];
    [self.view addSubview:_displayView];
    
    NSString *filePath = [[NSBundle mainBundle] pathForResource:@"Wizard" ofType:@"h264"];
    _hardDecoder = [[TFHardDecoder alloc] init];
    _hardDecoder.mediaUrl = [NSURL fileURLWithPath:filePath];
    
    __weak typeof(self) weakSelf = self;
    [_hardDecoder startWithFrameHandler:^(CVPixelBufferRef pixelBuffer) {
        CVPixelBufferRef copy = CVPixelBufferRetain(pixelBuffer);
        dispatch_async(dispatch_get_main_queue(), ^{
            [weakSelf.displayView displayPixelBuffer:copy];
        });
    }];
}

@end
