//
//  TFMPProgressView.m
//  TFMediaPlayer
//
//  Created by shiwei on 18/3/3.
//  Copyright © 2018年 shiwei. All rights reserved.
//

#import "TFMPProgressView.h"

@interface TFMPStickView : UIView

@property (nonatomic, assign) float rate;

@property (nonatomic, strong) UIColor *fillColor;

@property (nonatomic, strong) UIColor *normalColor;

@end

@implementation TFMPStickView

-(void)setRate:(float)rate{
    _rate = rate;
    
    [self setNeedsDisplay];
}

-(void)drawRect:(CGRect)rect{
    CGContextRef context = UIGraphicsGetCurrentContext();
    
    CGContextSetFillColorWithColor(context, _fillColor.CGColor);
    CGContextFillRect(context, CGRectMake(0, (rect.size.height-rect.size.height)/2.0f, rect.size.width*_rate, rect.size.height));
    
    CGContextSetFillColorWithColor(context, _normalColor.CGColor);
    CGContextFillRect(context, CGRectMake(rect.size.width*_rate, (rect.size.height-rect.size.height)/2.0f, rect.size.width*(1-_rate), rect.size.height));
}

@end


#pragma mark -

static CGFloat TFMPIndicatorRadius = 5;
static CGFloat TFMPStickHeight = 2;
static CGFloat TFMPTimeLabelWidth = 50;

@interface TFMPProgressView (){
    
    TFMPStickView *_stickView;
    
    UIView *_indicator;
    
    UILabel *_curTimeLabel;
    UILabel *_durationLabel;
    
    BOOL _touching;
}

@end

@implementation TFMPProgressView

-(instancetype)initWithFrame:(CGRect)frame{
    if ( self = [super initWithFrame:frame]) {
        
        [self setDefaultValues];
        [self setupSubViews];
    }
    
    return self;
}

-(void)setDefaultValues{
    _fillColor = [UIColor redColor];
    _normalColor = [UIColor colorWithWhite:0.95 alpha:1];
}

-(void)setupSubViews{
    
    _stickView = [[TFMPStickView alloc] init];
    _stickView.normalColor = _normalColor;
    _stickView.fillColor = _fillColor;
    [self addSubview:_stickView];
    
    _indicator = [[UIView alloc] initWithFrame:CGRectMake(0, 0, TFMPIndicatorRadius*2, TFMPIndicatorRadius*2)];
    _indicator.layer.borderColor = [UIColor colorWithWhite:0.3 alpha:1].CGColor;
    _indicator.layer.borderWidth = 0.5;
    _indicator.layer.cornerRadius = TFMPIndicatorRadius;
    _indicator.layer.masksToBounds = YES;
    
    _indicator.backgroundColor = [UIColor whiteColor];
    [self addSubview:_indicator];
    
    _curTimeLabel = [[UILabel alloc] init];
    _curTimeLabel.textColor = [UIColor whiteColor];
    _curTimeLabel.font = [UIFont systemFontOfSize:12];
    _curTimeLabel.textAlignment = NSTextAlignmentRight;
    [self addSubview:_curTimeLabel];
    
    _durationLabel = [[UILabel alloc] init];
    _durationLabel.textColor = [UIColor whiteColor];
    _durationLabel.font = [UIFont systemFontOfSize:12];
    _durationLabel.textAlignment = NSTextAlignmentLeft;
    [self addSubview:_durationLabel];
    
    self.duration = 0;
    self.currentTime = 0;
}

-(void)layoutSubviews{
    [super layoutSubviews];
    
    _stickView.frame = CGRectMake(TFMPTimeLabelWidth+10, (self.frame.size.height-TFMPStickHeight)/2.0f, self.frame.size.width - 2*(TFMPTimeLabelWidth+10), TFMPStickHeight);
    
    float rate = _duration > 0 ? _currentTime/_duration : 0;
    CGFloat stickWidth = CGRectGetWidth(_stickView.frame);
    _indicator.center = CGPointMake(CGRectGetMinX(_stickView.frame)+stickWidth*rate, self.frame.size.height/2.0);
    
    _curTimeLabel.frame = CGRectMake(0, 0, TFMPTimeLabelWidth, self.frame.size.height);
    _durationLabel.frame = CGRectMake(self.frame.size.width - TFMPTimeLabelWidth, 0, TFMPTimeLabelWidth, self.frame.size.height);
}

-(void)setCurrentTime:(float)currentTime{
    
    if (_touching) {
        return;
    }
    
    _currentTime = currentTime;
    [self _currentTimeChanged];
}

-(void)setDuration:(float)duration{
    _duration = duration;
    
    _durationLabel.text = [self timeTextFromInterval:_duration];
}

#pragma mark -

-(void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event{
    
    _curTimeLabel.textColor = [UIColor greenColor];
    _touching = YES;
}

-(void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event{
    CGPoint location = [touches.anyObject locationInView:self];
    
    float rate = location.x / self.frame.size.width;
    _currentTime = rate*_duration;
    
    NSLog(@"_currentTime: %.3f, rate:%.3f, duration: %.3f",_currentTime,rate,_duration);
    
    [self _currentTimeChanged];
    if (!_notifyWhenUntouch) {
        [self notifyWhenUntouch];
    }
}

-(void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event{
    
    //滚动的值和_currentTime并非同一个东西，滚动只是一个未来的期望值
    CGPoint location = [touches.anyObject locationInView:self];
    float rate = location.x / self.frame.size.width;
    
    if (_notifyWhenUntouch) {
        [self notifyValueChanged:rate*_duration];
    }
    
    _curTimeLabel.textColor = [UIColor whiteColor];
    _touching = NO;
}

-(void)_currentTimeChanged{
    float rate = _duration > 0 ? _currentTime/_duration : 0;
    CGFloat stickWidth = CGRectGetWidth(_stickView.frame);
    _indicator.center = CGPointMake(CGRectGetMinX(_stickView.frame)+stickWidth*rate, self.frame.size.height/2.0);
    
    _stickView.rate = rate;
    
    _curTimeLabel.text = [self timeTextFromInterval:_currentTime];
}

-(void)notifyValueChanged:(double)touchValue{
    if (self.valueChangedHandler) {
        self.valueChangedHandler(self, touchValue);
    }
}


-(NSString *)timeTextFromInterval:(double)interval{
    
    return [NSString stringWithFormat:@"%02d:%02d",(int)interval/60,(int)interval%60];
}

@end
