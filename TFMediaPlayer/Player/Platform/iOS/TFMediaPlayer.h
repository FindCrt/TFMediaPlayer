//
//  TFMediaPlayer.h
//  TFMediaPlayer
//
//  Created by shiwei on 17/12/25.
//  Copyright © 2017年 shiwei. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "TFAudioUnitPlayer.h"


typedef NS_ENUM(NSInteger, TFMediaPlayerState) {
    TFMediaPlayerStateNone,
    TFMediaPlayerStateConnecting,
    TFMediaPlayerStateReady,
    TFMediaPlayerStatePlaying,
    TFMediaPlayerStatePause,
};

@interface TFMediaPlayer : NSObject

/** setter value may isn't equal to getter value. Setter is the vale you desired, while getter is value that actual played.  */
@property (nonatomic, assign) TFMPMediaType mediaType;

@property (nonatomic, strong) NSURL *mediaURL;

@property (nonatomic, strong, readonly) UIView *displayView;

@property (nonatomic, strong) UIView *controlView;

-(void)preparePlay;

-(void)play;
-(void)pause;
-(void)stop;

@property (nonatomic, assign) TFMPShareAudioBufferStruct shareAudioStruct;

@property (nonatomic, assign, readonly) TFMediaPlayerState state;

@property (nonatomic, assign) BOOL autoPlayWhenReady;

@property (nonatomic, assign) double currentTime;

-(void)seekToPlayTime:(NSTimeInterval)playTime;

-(void)seekByForward:(NSTimeInterval)interval;

-(void)changeFullScreenState;

@end
