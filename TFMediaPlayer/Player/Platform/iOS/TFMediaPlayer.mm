//
//  TFMediaPlayer.m
//  TFMediaPlayer
//
//  Created by shiwei on 17/12/25.
//  Copyright © 2017年 shiwei. All rights reserved.
//

#define TFMPUseAudioUnitPlayer 0

#import "TFMediaPlayer.h"
#import "PlayController.hpp"
#import "TFOPGLESDisplayView.h"
#import "UIDevice+ForceChangeOrientation.h"
#import "TFMPPlayControlView.h"
#import "TFMPPlayCmdResolver.h"

#if TFMPUseAudioUnitPlayer
#import "TFAudioUnitPlayer.h"
#else
#import "TFAudioQueueController.h"
#endif

#import "TFMPDebugFuncs.h"


@interface TFMediaPlayer (){
    tfmpcore::PlayController *_playController;
    
    TFMPMediaType _mediaType;
    
    BOOL _innerPlayWhenReady;
    
#if TFMPUseAudioUnitPlayer
    TFAudioUnitPlayer *_audioPlayer;
#else
    TFAudioQueueController *_audioPlayer;
#endif
    
    TFOPGLESDisplayView *_displayView;
    
    NSURL *_nextMedia;
}

/**
 * it's a mark what indicates the user's intention: whatever it's state, don't play this media!
 * So it's different from the state of TFMediaPlayerStatePaused which equal to resource is fine + pauseMarked.
 */
@property (nonatomic, assign, readonly) BOOL pauseMarked;

@end

@implementation TFMediaPlayer

-(instancetype)init{
    if (self = [super init]) {
        
        _displayView = [[TFOPGLESDisplayView alloc] init];
        
        _playController = new tfmpcore::PlayController();
        
        _playController->setDesiredDisplayMediaType(TFMP_MEDIA_TYPE_ALL_AVIABLE);
        _playController->isAudioMajor = true;
        
        _playController->displayContext = (__bridge void *)self;
        _playController->displayVideoFrame = displayVideoFrame_iOS;
        
        _playController->connectCompleted = [self](tfmpcore::PlayController *playController){
            
            if (self.state == TFMediaPlayerStateConnecting) {
                self.state = TFMediaPlayerStateReady;
            }
            
            if (_innerPlayWhenReady || _autoPlayWhenReady) {
                [self play];
            }
        };
        
        _playController->playStoped = [self](tfmpcore::PlayController *playController, int reason){
            
            if (reason == 0 && _state != TFMediaPlayerStateStoping) {
                [self stop];
            }else if (reason == 1 && self.state == TFMediaPlayerStateStoping){
                self.state = TFMediaPlayerStateStoped;
                if (_nextMedia) {  //a new media is waiting to play.
                    _mediaURL = _nextMedia;
                    _nextMedia = nil;
                    [self preparePlay];
                }
            }
        };
        
        _playController->seekingEndNotify = [self](tfmpcore::PlayController *playController){
            
            if (_pauseMarked) { //The user intent to don't play, so pause it.
                self.state = TFMediaPlayerStatePaused;
            }else{
                self.state = TFMediaPlayerStatePlaying;
            }
        };
        
        _playController->bufferingStateChanged = [self](tfmpcore::PlayController *playController, bool isBuffering){
            if (isBuffering) {
                [_audioPlayer pause];
            }else{
                [_audioPlayer play];
            }
        };
        
        _playController->negotiateAdoptedPlayAudioDesc = [self](TFMPAudioStreamDescription sourceDesc){
#if TFMPUseAudioUnitPlayer
            _audioPlayer = [[TFAudioUnitPlayer alloc] init];
            _audioPlayer.fillStruct = _playController->getFillAudioBufferStruct();
            TFMPAudioStreamDescription result = [_audioPlayer resultAudioDescForSource:sourceDesc];
            return result;
#else
            _audioPlayer = [[TFAudioQueueController alloc] initWithSpecifics:sourceDesc];
            _audioPlayer.shareAudioStruct = _shareAudioStruct;
            _audioPlayer.fillStruct = _playController->getFillAudioBufferStruct();
            
            return _audioPlayer.resultSpecifics;
#endif
        };
        
        [self setupDefaultPlayControlView];
    }
    
    return self;
}

-(void)setupDefaultPlayControlView{
    _defaultControlView = [[TFMPPlayControlView alloc] init];
    _defaultControlView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    
    TFMPPlayCmdResolver *defaultPlayResolver = [[TFMPPlayCmdResolver alloc] init];
    defaultPlayResolver.player = self;
    _defaultControlView.delegate = defaultPlayResolver;
    
    _defaultControlView.swipeSeekDuration = 15;
    
    self.controlView = _defaultControlView;
}

-(void)setControlView:(UIView *)controlView{
    if (_controlView == controlView) {
        return;
    }
    
    if (_controlView) {
        [_controlView removeFromSuperview];
    }
    
    _controlView = controlView;
    _controlView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    
    [self.displayView addSubview:_controlView];
}

-(UIView *)displayView{
    return _displayView;
}

-(void)setMediaURL:(NSURL *)mediaURL{
    if (_mediaURL == mediaURL) {
        return;
    }
    
//    if (_state != TFMediaPlayerStateNone &&
//        _state != TFMediaPlayerStateStoped) {
//        [self switchToNewMedia:mediaURL];
//    }
    
    _mediaURL = mediaURL;
}

-(void)setMediaType:(TFMPMediaType)mediaType{
    _mediaType = mediaType;
    _playController->setDesiredDisplayMediaType(mediaType);
    
    
//    if ((_playController->getRealDisplayMediaType() & TFMP_MEDIA_TYPE_AUDIO)) {
//        if (_audioPlayer.state == TFAudioQueueStateUnplay) {
//            [_audioPlayer play];
//        }
//    }else{
//        if (_audioPlayer.state == TFAudioQueueStatePlaying) {
//            [_audioPlayer stop];
//        }
//    }
}

-(TFMPMediaType)mediaType{
    return _mediaType;
}

-(void)setState:(TFMediaPlayerState)state{
    if (_state == state) {
        return;
    }
    _state = state;
    
    dispatch_async(dispatch_get_main_queue(), ^{
        [[NSNotificationCenter defaultCenter] postNotificationName:TFMPStateChangedNotification object:self userInfo:@{TFMPStateChangedKey:@(state)}];
    });
    
    myStateObserver.mark("play state", state);
}

-(BOOL)isPlaying{
    return self.state == TFMediaPlayerStatePlaying;
}

#pragma mark -

-(void)preparePlay{
    
    if (_mediaURL == nil) {
        self.state = TFMediaPlayerStateNone;
        return;
    }
    
    self.state = TFMediaPlayerStateConnecting;
    
    dispatch_async(dispatch_get_global_queue(0, 0), ^{
        //local file or bundle file need a file reader which is different on different platform.
        bool succeed = _playController->connectAndOpenMedia([[_mediaURL absoluteString] cStringUsingEncoding:NSUTF8StringEncoding]);
        if (!succeed) {
            TFMPDLog(@"play media open error");
            self.state = TFMediaPlayerStateNone;
        }
    });
}

-(void)play{
    
    _pauseMarked = false;
    
    if (![self configureAVSession]) {
        self.state = TFMediaPlayerStateNone;
        return;
    }
    
    if (_playController->getRealDisplayMediaType() != _mediaType) {
        _playController->setDesiredDisplayMediaType(_mediaType);
    }
    
    if (self.state == TFMediaPlayerStateNone) {
        
        _innerPlayWhenReady = YES;
        [self preparePlay];
        
    }else if (self.state == TFMediaPlayerStateConnecting) {
        _innerPlayWhenReady = YES;
    }else if (self.state == TFMediaPlayerStateReady){
        
        _playController->play();
        
        if (_playController->getRealDisplayMediaType() & TFMP_MEDIA_TYPE_AUDIO) {
            [_audioPlayer play];
        }
        
        self.state = TFMediaPlayerStatePlaying;
    }else if(self.state == TFMediaPlayerStatePaused){
        
        if (_playController->getRealDisplayMediaType() & TFMP_MEDIA_TYPE_AUDIO) {
            [_audioPlayer play];
        }
        
        _playController->pause(false);
        
        self.state = TFMediaPlayerStatePlaying;
    }
}

-(void)pause{
    
    _pauseMarked = true;
    
    switch (_state) {
        case TFMediaPlayerStateConnecting:
            //TODO: stop connecting
            _autoPlayWhenReady = false;
            _innerPlayWhenReady = false;
            break;
        case TFMediaPlayerStateReady:
            _autoPlayWhenReady = false;
            _innerPlayWhenReady = false;
            break;
        case TFMediaPlayerStatePlaying:
        case TFMediaPlayerStateLoading:
            _playController->pause(true);
            if (_playController->getRealDisplayMediaType() & TFMP_MEDIA_TYPE_AUDIO) {
                [_audioPlayer pause];
            }
            self.state = TFMediaPlayerStatePaused;
            
            break;
        default:
            break;
    }
    
}

-(void)stop{
    
    [_audioPlayer stop];
    
    switch (_state) {
        case TFMediaPlayerStateConnecting:
            _autoPlayWhenReady = false;
            _innerPlayWhenReady = false;
            _playController->stop();
            break;
        case TFMediaPlayerStateReady:
            _autoPlayWhenReady = false;
            _innerPlayWhenReady = false;
            _playController->stop();
            break;
        case TFMediaPlayerStatePlaying:
        case TFMediaPlayerStateLoading:
            _playController->stop();
            break;
        default:
            break;
    }
    
    _pauseMarked = false;
    self.state = TFMediaPlayerStateStoping;
}

-(void)switchToNewMedia:(NSURL *)mediaURL{
    
    if (_state == TFMediaPlayerStateNone ||
        _state == TFMediaPlayerStateStoped) {
        self.mediaURL = mediaURL;
        [self play];
    }else{
        _nextMedia = mediaURL;
        [self stop];
    }
}

-(BOOL)configureAVSession{
    
//    NSError *error = nil;
//
//    [[AVAudioSession sharedInstance]setCategory:AVAudioSessionCategoryPlayback error:&error];
//    if (error) {
//        TFMPDLog(@"audio session set category error: %@",error);
//        return NO;
//    }
//
//    [[AVAudioSession sharedInstance] setActive:YES error:&error];
//    if (error) {
//        TFMPDLog(@"active audio session error: %@",error);
//        return NO;
//    }
    
    return YES;
}

-(void)setShareAudioStruct:(TFMPShareAudioBufferStruct)shareAudioStruct{
    _shareAudioStruct = shareAudioStruct;
    [_audioPlayer setShareAudioStruct:shareAudioStruct];
}

-(double)duration{
    return _playController->getDuration();
}

#pragma mark - platform special

int displayVideoFrame_iOS(TFMPVideoFrameBuffer *frameBuf, void *context){
    TFMediaPlayer *player = (__bridge TFMediaPlayer *)context;
    
    [player->_displayView displayFrameBuffer:frameBuf];
    return 0;
}


#pragma mark - controls

-(void)seekToPlayTime:(NSTimeInterval)playTime{
    if (_state == TFMediaPlayerStateNone ||
        _state == TFMediaPlayerStateConnecting ||
        _state == TFMediaPlayerStateReady) {
        return;
    }
    _playController->seekTo(playTime);
    self.state = TFMediaPlayerStateLoading;
}

-(void)seekByForward:(NSTimeInterval)interval{
    if (_state == TFMediaPlayerStateNone ||
        _state == TFMediaPlayerStateConnecting ||
        _state == TFMediaPlayerStateReady) {
        return;
    }
    
    _playController->seekByForward(interval);
    self.state = TFMediaPlayerStateLoading;
}

-(void)changeFullScreenState{
    if ([UIDevice currentDevice].orientation == UIDeviceOrientationPortrait || [UIDevice currentDevice].orientation == UIDeviceOrientationPortraitUpsideDown) {
        [UIDevice changeOrientationTo:(UIDeviceOrientationLandscapeRight)];
    }else{
        [UIDevice changeOrientationTo:(UIDeviceOrientationPortrait)];
    }
}

-(double)currentTime{
    if (self.state == TFMediaPlayerStatePlaying ||
        self.state == TFMediaPlayerStatePaused ||
        self.state == TFMediaPlayerStateLoading) {
        return _playController->getCurrentTime();
    }else{
        return 0;
    }
}

@end
