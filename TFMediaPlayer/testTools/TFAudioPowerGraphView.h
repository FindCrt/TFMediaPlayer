//
//  TFAudioPowerGraphView.h
//  TFMediaPlayer
//
//  Created by shiwei on 18/1/12.
//  Copyright © 2018年 shiwei. All rights reserved.
//

#import <UIKit/UIKit.h>

static int APGOnePagePoints = 1024;

@interface TFAudioPowerGraphView : UIView

@property (nonatomic, assign) BOOL ignoreSign; //

@property (nonatomic, assign) BOOL changeColor;

@property (nonatomic, assign) int colorFlagCycleCount;

@property (nonatomic, assign) BOOL changeBGColor;

@property (nonatomic, assign) float showRate;

@property (nonatomic, assign) int bytesPerSample;

@property (nonatomic, assign) uint64_t sampleRate;

-(void)showBuffer:(void *)buffer size:(uint32_t)size;

-(void)start;

-(void)stop;

@end


@interface TFAudioPowerGraphContentView : UIView

@property (nonatomic, assign) NSInteger id;

@property (nonatomic, assign) float value;

@property (nonatomic, assign) BOOL horizontal;

@property (nonatomic, strong) UIColor *fillColor;

@property (nonatomic, assign) BOOL ignoreSign; //

@end
