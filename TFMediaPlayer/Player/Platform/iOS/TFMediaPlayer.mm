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
#import "VTBDecoder.h"

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
    
    UIView *_stopedView;
}

/**
 * it's a mark what indicates the user's intention: whatever it's state, don't play this media!
 * So it's different from the state of TFMediaPlayerStatePaused which equal to resource is fine + pauseMarked.
 */
@property (nonatomic, assign, readonly) BOOL pauseMarked;

@end

@implementation TFMediaPlayer

-(instancetype)init{
    return [self initWithParams:nil];
}

-(instancetype)initWithParams:(NSDictionary *)params{
    if (self = [super init]) {
        
        [self setValuesForKeysWithDictionary:params];
        
        _displayView = [[TFOPGLESDisplayView alloc] init];
        
        _playController = new tfmpcore::PlayController();
        
        _playController->setDesiredDisplayMediaType(TFMP_MEDIA_TYPE_ALL_AVIABLE);
        
        _playController->displayContext = (__bridge void *)self;
        _playController->displayVideoFrame = displayVideoFrame_iOS;
//        _playController->clockMajor = TFMP_SYNC_CLOCK_MAJOR_VIDEO;
//        _playController->accurateSeek = false;
        
        if (_activeVTB) {
            _playController->setVideoDecoder(new tfmpcore::VTBDecoder());
        }

        __weak typeof(self) weakSelf = self;
        _playController->playStoped = [weakSelf](tfmpcore::PlayController *playController, int reason){

            __strong typeof(weakSelf) self = weakSelf;

            if (reason == TFMP_STOP_REASON_END_OF_FILE && _state != TFMediaPlayerStateStoping) {
                [self stop];
            }else if (reason == TFMP_STOP_REASON_USER_STOP && self.state == TFMediaPlayerStateStoping){
                self.state = TFMediaPlayerStateStoped;

                if (_nextMedia) {  //a new media is waiting to play.
                    _mediaURL = _nextMedia;
                    _nextMedia = nil;
                    [self preparePlay];
                }else{
                    dispatch_async(dispatch_get_main_queue(), ^{
                        self.stopedView.hidden = NO;
                    });
                }
            }
        };

        _playController->seekingEndNotify = [weakSelf](tfmpcore::PlayController *playController){
            __strong typeof(weakSelf) self = weakSelf;
            dispatch_async(dispatch_get_main_queue(), ^{
                if (_pauseMarked) { //The user intent to don't play, so pause it.
                    self.state = TFMediaPlayerStatePaused;
                }else{
                    self.state = TFMediaPlayerStatePlaying;
                }
            });
        };

        _playController->bufferingStateChanged = [weakSelf](tfmpcore::PlayController *playController, bool isBuffering){
//            __strong typeof(weakSelf) self = weakSelf;
//            if (isBuffering) {
//                [_audioPlayer pause];
//            }else{
//                [_audioPlayer play];
//            }
        };

        _playController->negotiateAdoptedPlayAudioDesc = [weakSelf](TFMPAudioStreamDescription sourceDesc){

            __strong typeof(weakSelf) self = weakSelf;
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
        self.stopedView = self.stopedView;
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

-(void)setStopedView:(UIView *)stopedView{
    _stopedView = stopedView;
    _stopedView.hidden = YES;

    [self.displayView insertSubview:_stopedView belowSubview:_controlView];
}

-(UIView *)stopedView{
    if (!_stopedView) {
        UILabel *label = [[UILabel alloc] initWithFrame:self.displayView.bounds];
        label.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
        label.font = [UIFont boldSystemFontOfSize:20];
        label.textColor = [UIColor redColor];
        label.textAlignment = NSTextAlignmentCenter;
        label.text = @"Game Over";

        _stopedView = label;
    }

    return _stopedView;
}

-(UIView *)displayView{
    return _displayView;
}

-(void)setMediaURL:(NSURL *)mediaURL{
    if (_mediaURL == mediaURL) {
        return;
    }
    
    if (_state != TFMediaPlayerStateNone &&
        _state != TFMediaPlayerStateStoped) {
        [self switchToNewMedia:mediaURL];
    }
    
    _mediaURL = mediaURL;
}

-(void)setMediaType:(TFMPMediaType)mediaType{
    _mediaType = mediaType;
    _playController->setDesiredDisplayMediaType(mediaType);
}

-(TFMPMediaType)mediaType{
    return _mediaType;
}

-(void)setState:(TFMediaPlayerState)state{
    if (_state == state) {
        return;
    }
    _state = state;
    myStateObserver.mark("play state", state);

    dispatch_async(dispatch_get_main_queue(), ^{
        [[NSNotificationCenter defaultCenter] postNotificationName:TFMPStateChangedNotification object:self userInfo:@{TFMPStateChangedKey:@(state)}];
    });
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
        }else{
            self.state = TFMediaPlayerStateReady;
            
            if (_innerPlayWhenReady || _autoPlayWhenReady) {
                [self play];
            }
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
    
    dispatch_async(dispatch_get_main_queue(), ^{
        self.stopedView.hidden = YES;
    });
    
    if (self.state == TFMediaPlayerStateNone ||
        self.state == TFMediaPlayerStateStoped) {
        
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
            _playController->cancelConnecting();
            break;
        case TFMediaPlayerStateReady:
        {
            _autoPlayWhenReady = false;
            _innerPlayWhenReady = false;
            dispatch_async(dispatch_get_global_queue(0, 0), ^{
                _playController->stop();
            });
        }
            break;
        case TFMediaPlayerStatePlaying:
        case TFMediaPlayerStateLoading:
        {
            dispatch_async(dispatch_get_global_queue(0, 0), ^{
                _playController->stop();
            });
        }
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
        _state == TFMediaPlayerStateReady ||
        _state == TFMediaPlayerStateStoped) {
        return;
    }
    _playController->seekTo(playTime);
    self.state = TFMediaPlayerStateLoading;
}

-(void)seekByForward:(NSTimeInterval)interval{
    if (_state == TFMediaPlayerStateNone ||
        _state == TFMediaPlayerStateConnecting ||
        _state == TFMediaPlayerStateReady ||
        _state == TFMediaPlayerStateStoped) {
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

-(void)dealloc{
    delete _playController;
    NSLog(@"%@ dealloc",[self class]);
}

@end
