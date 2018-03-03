//
//  TFMPPlayControlView.m
//  TFMediaPlayer
//
//  Created by shiwei on 2018/2/15.
//  Copyright © 2018年 shiwei. All rights reserved.
//

#import "TFMPPlayControlView.h"
#import "TFMPProgressView.h"

#import "TFMPDebugFuncs.h"

#define TFMPCVWidth (self.bounds.size.width)
#define TFMPCVHeight (self.bounds.size.height)

static CGFloat TFMPCVHorizontalSpace = 10;
static CGFloat TFMPCVBottomSpace = 10;

static CGFloat TFMPCVFullScreenWidth = 32;

@interface TFMPPlayControlView()<UIGestureRecognizerDelegate>{
    UISwipeGestureRecognizer *_swipe;
    
    UIButton *_fullScreenButton;
    
    TFMPProgressView *_progressView;
}

@property (nonatomic, assign) double duration;

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
    
    _progressView.frame = CGRectMake(40, TFMPCVHeight - TFMPCVBottomSpace - 30, CGRectGetMinX(_fullScreenButton.frame)-50, 30);
}

-(void)setupFullScreenControl{
    _fullScreenButton = [[UIButton alloc] initWithFrame:CGRectMake(0, 0, 32, 32)];
    [_fullScreenButton setImage:[UIImage imageNamed:@"full_screen"] forState:(UIControlStateNormal)];
    [_fullScreenButton addTarget:self action:@selector(fullScrrenAction) forControlEvents:(UIControlEventTouchUpInside)];
    [self addSubview:_fullScreenButton];
}

-(void)setupSeekControls{
    _swipe = [[UISwipeGestureRecognizer alloc] initWithTarget:self action:@selector(swipeScreen:)];
    _swipe.delegate = self;
    
    [self addGestureRecognizer:_swipe];
    
    _progressView = [[TFMPProgressView alloc] init];
    _progressView.notifyWhenUntouch = YES;
    
    __weak typeof(self) weakSelf = self;
    [_progressView setValueChangedHandler:^(TFMPProgressView *progressView) {
        
        if (weakSelf.duration <= 0) {
            return;
        }
        double time = progressView.currentTime;
        
        //seek to a time point.
        [weakSelf.delegate dealPlayControlCommand:TFMPCmd_seek_TP params:@{TFMPCmd_param_time : @(time) }];
    }];
    [self addSubview:_progressView];
}



#pragma mark - actions

-(void)fullScrrenAction{
    [self.delegate dealPlayControlCommand:TFMPCmd_fullScreen params:nil];
}

-(BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer *)gestureRecognizer{
    CGPoint location = [gestureRecognizer locationInView:_progressView];
    if (CGRectContainsPoint(_progressView.bounds, location)) {
        return NO;
    }
    
    return YES;
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

#pragma mark - state change

-(void)setDelegate:(id<TFMPControlProtocol>)delegate{
    _delegate = delegate;
    
    NSArray *states = @[TFMPState_duration, TFMPState_currentTime];
    [_delegate helpObserveStates:states withChangedHandler:^(NSString *state, id value) {
        [self playState:state changedTo:value];
    }];
}

-(void)playState:(NSString *)state changedTo:(id)value{
    
    if ([state isEqualToString:TFMPState_duration]) {
        
        _duration = [value doubleValue];
        _progressView.duration = _duration;
        
    }else if ([state isEqualToString:TFMPState_currentTime]){
        
        if (_duration <= 0) {
            return;
        }
        
        double currentTime = [value doubleValue];
        _progressView.currentTime = currentTime;
    }
}

@end
