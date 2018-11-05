//
//  TFMediaPlayer.h
//  TFMediaPlayer
//
//  Created by shiwei on 17/12/25.
//  Copyright © 2017年 shiwei. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "TFAudioUnitPlayer.h"
#import "TFMPPlayControlView.h"

typedef NS_ENUM(NSInteger, TFMediaPlayerState) {
    TFMediaPlayerStateNone,    //playing is unstart or stoped.
    TFMediaPlayerStateConnecting,
    TFMediaPlayerStateReady,
    TFMediaPlayerStatePlaying,
    TFMediaPlayerStatePaused,
    TFMediaPlayerStateLoading,   //net is not good or seeking to new position.
    TFMediaPlayerStateStoping,  //The player is stoping, actually the main task is releasing media sources.
    TFMediaPlayerStateStoped
};

static NSString *TFMPStateChangedKey = @"state";
static NSString *TFMPStateChangedNotification = @"TFMPStateChangedNotification";

@interface TFMediaPlayer : NSObject

/** params的key使用当前类的属性名，用来设置初始值，如activeVTB */
-(instancetype)initWithParams:(NSDictionary *)params;

/** setter value may isn't equal to getter value. Setter is the vale you desired, while getter is value that actual played.  */
@property (nonatomic, assign) TFMPMediaType mediaType;

@property (nonatomic, strong) NSURL *mediaURL;

@property (nonatomic, strong, readonly) UIView *displayView;

/** 是否激活VideoToolBox来使用硬解视频 */
@property (nonatomic, readonly) BOOL activeVTB;

-(void)preparePlay;

-(void)play;
-(void)pause;
-(void)stop;

/** Call this func to switch media instead if using stop+play because stop actually is aync. */
-(void)switchToNewMedia:(NSURL *)mediaURL;

@property (nonatomic, assign) TFMPShareAudioBufferStruct shareAudioStruct;

@property (nonatomic, assign, readonly) TFMediaPlayerState state;

@property (nonatomic, assign, readonly) BOOL isPlaying;

@property (nonatomic, assign) BOOL autoPlayWhenReady;

@property (nonatomic, assign, readonly) double duration;

@property (nonatomic, assign) double currentTime;

-(void)seekToPlayTime:(NSTimeInterval)playTime;

-(void)seekByForward:(NSTimeInterval)interval;

-(void)changeFullScreenState;


#pragma mark - control property

@property (nonatomic, strong, readonly) TFMPPlayControlView *defaultControlView;

@property (nonatomic, strong) UIView *controlView;

@property (nonatomic) UIView *stopedView;

@end
