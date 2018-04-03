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
    UISwipeGestureRecognizer *_swipeRight;
    UISwipeGestureRecognizer *_swipeLeft;
    
    UITapGestureRecognizer *_doubleTap;
    
    UIButton *_fullScreenButton;
    
    TFMPProgressView *_progressView;
    
    UIActivityIndicatorView *_actIndicator;
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
    [self setupTapControl];
    
    _actIndicator = [[UIActivityIndicatorView alloc] initWithActivityIndicatorStyle:(UIActivityIndicatorViewStyleWhiteLarge)];
    _actIndicator.hidesWhenStopped = YES;
    [self addSubview:_actIndicator];
}

-(void)layoutSubviews{
    [super layoutSubviews];
    
    _fullScreenButton.frame = CGRectMake(TFMPCVWidth-TFMPCVHorizontalSpace-TFMPCVFullScreenWidth, TFMPCVHeight-TFMPCVBottomSpace-TFMPCVFullScreenWidth, TFMPCVFullScreenWidth, TFMPCVFullScreenWidth);
    
    _progressView.frame = CGRectMake(40, TFMPCVHeight - TFMPCVBottomSpace - 30, CGRectGetMinX(_fullScreenButton.frame)-50, 30);
    
    _actIndicator.center = CGPointMake(self.bounds.size.width/2.0, self.bounds.size.height/2.0);
}

-(void)setupFullScreenControl{
    _fullScreenButton = [[UIButton alloc] initWithFrame:CGRectMake(0, 0, 32, 32)];
    [_fullScreenButton setImage:[UIImage imageNamed:@"full_screen"] forState:(UIControlStateNormal)];
    [_fullScreenButton addTarget:self action:@selector(fullScrrenAction) forControlEvents:(UIControlEventTouchUpInside)];
    [self addSubview:_fullScreenButton];
}

-(void)setupSeekControls{
    _swipeLeft = [[UISwipeGestureRecognizer alloc] initWithTarget:self action:@selector(swipeScreen:)];
    _swipeLeft.delegate = self;
    _swipeLeft.direction = UISwipeGestureRecognizerDirectionLeft;
    [self addGestureRecognizer:_swipeLeft];
    
    _swipeRight = [[UISwipeGestureRecognizer alloc] initWithTarget:self action:@selector(swipeScreen:)];
    _swipeRight.delegate = self;
    [self addGestureRecognizer:_swipeRight];
    
    _progressView = [[TFMPProgressView alloc] init];
    _progressView.notifyWhenUntouch = YES;
    
    __weak typeof(self) weakSelf = self;
    [_progressView setValueChangedHandler:^(TFMPProgressView *progressView, double seekTime) {
        
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (strongSelf.duration <= 0) {
            return;
        }
        
        [strongSelf->_actIndicator startAnimating];
        
        //seek to a time point.
        [strongSelf.delegate dealPlayControlCommand:TFMPCmd_seek_TP params:@{TFMPCmd_param_time : @(seekTime) }];
    }];
    [self addSubview:_progressView];
}

-(void)setupTapControl{
    _doubleTap = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(doubleTapAction:)];
    _doubleTap.numberOfTapsRequired = 2;
    [self addGestureRecognizer:_doubleTap];
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
    
    [_actIndicator startAnimating];
    
    if (swipe.direction == UISwipeGestureRecognizerDirectionRight) {
        TFMPDLog(@"forward 10s");
        [self.delegate dealPlayControlCommand:TFMPCmd_seek_TD params:@{TFMPCmd_param_duration : @(_swipeSeekDuration)}];
    }else if (swipe.direction == UISwipeGestureRecognizerDirectionLeft){
        TFMPDLog(@"back 10s");
        [self.delegate dealPlayControlCommand:TFMPCmd_seek_TD params:@{TFMPCmd_param_duration : @(-_swipeSeekDuration)}];
    }
}

-(void)doubleTapAction:(UITapGestureRecognizer *)tap{
    [self.delegate dealPlayControlCommand:TFMPCmd_double_tap params:nil];
}

#pragma mark - state change

-(void)setDelegate:(id<TFMPControlProtocol>)delegate{
    _delegate = delegate;
    
    NSArray *states = @[TFMPState_duration, TFMPState_currentTime, TFMPState_isLoading];
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
//        NSLog(@"currentTime: %.3f\n",currentTime);
    }else if ([state isEqualToString:TFMPState_isLoading]){
        
        if ([value boolValue]) {
            [_actIndicator startAnimating];
        }else{
            [_actIndicator stopAnimating];
        }
    }
}

@end
