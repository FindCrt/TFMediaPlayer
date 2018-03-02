//
//  TFMPPlayControlView.m
//  TFMediaPlayer
//
//  Created by shiwei on 2018/2/15.
//  Copyright © 2018年 shiwei. All rights reserved.
//

#import "TFMPPlayControlView.h"
#import "TFMPDebugFuncs.h"

#define TFMPCVWidth (self.bounds.size.width)
#define TFMPCVHeight (self.bounds.size.height)

static CGFloat TFMPCVHorizontalSpace = 10;
static CGFloat TFMPCVBottomSpace = 10;

static CGFloat TFMPCVFullScreenWidth = 32;

@interface TFMPPlayControlView(){
    UISwipeGestureRecognizer *_swipe;
    
    UIButton *_fullScreenButton;
}

@end

@implementation TFMPPlayControlView

-(instancetype)initWithFrame:(CGRect)frame{
    if (self = [super initWithFrame:frame]) {
        
        _swipeSeekDuration = 10;
        
        [self setupSubViews];
    }
    
    return self;
}

-(void)setupSubViews{
    
    [self setupFullScreenControl];
    [self setupSeekControls];
}

-(void)layoutSubviews{
    [super layoutSubviews];
    
    _fullScreenButton.frame = CGRectMake(TFMPCVWidth-TFMPCVHorizontalSpace-TFMPCVFullScreenWidth, TFMPCVHeight-TFMPCVBottomSpace-TFMPCVFullScreenWidth, TFMPCVFullScreenWidth, TFMPCVFullScreenWidth);
}

-(void)setupFullScreenControl{
    _fullScreenButton = [[UIButton alloc] initWithFrame:CGRectMake(0, 0, 32, 32)];
    [_fullScreenButton setImage:[UIImage imageNamed:@"full_screen"] forState:(UIControlStateNormal)];
    [_fullScreenButton addTarget:self action:@selector(fullScrrenAction) forControlEvents:(UIControlEventTouchUpInside)];
    [self addSubview:_fullScreenButton];
}

-(void)setupSeekControls{
    _swipe = [[UISwipeGestureRecognizer alloc] initWithTarget:self action:@selector(swipeScreen:)];
    
    [self addGestureRecognizer:_swipe];
}

#pragma mark - actions

-(void)fullScrrenAction{
    [self.delegate dealPlayControlCommand:TFMPCmd_fullScreen params:nil];
}

-(void)swipeScreen:(UISwipeGestureRecognizer *)swipe{
    if (swipe.direction == UISwipeGestureRecognizerDirectionRight) {
        TFMPDLog(@"forward 10s");
        [self.delegate dealPlayControlCommand:TFMPCmd_seek_TD params:@{TFMPCmd_param_duration : @(_swipeSeekDuration)}];
    }else if (swipe.direction == UISwipeGestureRecognizerDirectionLeft){
        TFMPDLog(@"back 10s");
        [self.delegate dealPlayControlCommand:TFMPCmd_seek_TD params:@{TFMPCmd_param_duration : @(-_swipeSeekDuration)}];
    }
}

@end
