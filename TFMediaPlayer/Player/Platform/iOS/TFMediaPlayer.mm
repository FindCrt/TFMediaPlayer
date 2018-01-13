//
//  TFMediaPlayer.m
//  TFMediaPlayer
//
//  Created by shiwei on 17/12/25.
//  Copyright © 2017年 shiwei. All rights reserved.
//

#import "TFMediaPlayer.h"
#import "PlayController.hpp"
#import "TFOPGLESDisplayView.h"
#import "TFAudioUnitPlayer.h"
#import "TFMPDebugFuncs.h"

//#import <AVFoundation/AVFoundation.h>

@interface TFMediaPlayer (){
    tfmpcore::PlayController *_playController;
    
    BOOL _innerPlayWhenReady;
    
    TFOPGLESDisplayView *_displayView;
    TFAudioUnitPlayer *_audioPlayer;
}

@end

@implementation TFMediaPlayer

-(instancetype)init{
    if (self = [super init]) {
        
        _displayView = [[TFOPGLESDisplayView alloc] init];
        
        _playController = new tfmpcore::PlayController();
        
        _playController->setDesiredDisplayMediaType(TFMP_MEDIA_TYPE_ALL_AVIABLE);
        _playController->isAudioMajor = false;
        
        _playController->displayContext = (__bridge void *)self;
        _playController->displayVideoFrame = displayVideoFrame_iOS;
        
        _playController->connectCompleted = [self](tfmpcore::PlayController *playController){
            
            if (_state == TFMediaPlayerStateConnecting) {
                _state = TFMediaPlayerStateReady;
            }
            
            if (_innerPlayWhenReady || _autoPlayWhenReady) {
                [self play];
            }
        };
        
        
        _audioPlayer = [[TFAudioUnitPlayer alloc] init];
        _audioPlayer.fillStruct = _playController->getFillAudioBufferStruct();
        
        _playController->negotiateAdoptedPlayAudioDesc = [self](TFMPAudioStreamDescription sourceDesc){
            
            TFMPAudioStreamDescription result = [_audioPlayer resultAudioDescForSource:sourceDesc];
            
            return result;
        };
    }
    
    return self;
}

-(UIView *)displayView{
    return _displayView;
}

-(void)setMediaURL:(NSURL *)mediaURL{
    if (_mediaURL == mediaURL) {
        return;
    }
    
    _mediaURL = mediaURL;
    if (_state != TFMediaPlayerStateNone) {
        [self stop];
    }
}

-(void)preparePlay{
    
    if (_mediaURL == nil) {
        _state = TFMediaPlayerStateNone;
        return;
    }
    
    _state = TFMediaPlayerStateConnecting;
    
    //local file or bundle file need a file reader which is different on different platform.
    bool succeed = _playController->connectAndOpenMedia([[_mediaURL absoluteString] cStringUsingEncoding:NSUTF8StringEncoding]);
    if (!succeed) {
        NSLog(@"play media open error");
    }
}

-(void)play{
    
    if (![self configureAVSession]) {
        _state = TFMediaPlayerStateNone;
        return;
    }
    
    if (_state == TFMediaPlayerStateNone) {
        
        _innerPlayWhenReady = YES;
        [self preparePlay];
        
    }else if (_state == TFMediaPlayerStateConnecting) {
        _innerPlayWhenReady = YES;
    }else if (_state == TFMediaPlayerStateReady || _state == TFMediaPlayerStatePause){
        
        _playController->play();
        [_audioPlayer play];
        
        _state = TFMediaPlayerStatePlaying;
    }
}

-(void)pause{
    
}

-(void)stop{
    
    switch (_state) {
        case TFMediaPlayerStateConnecting:
            _playController->stop();
            break;
        case TFMediaPlayerStateReady:
            _playController->stop();
            break;
        case TFMediaPlayerStatePlaying:
            _playController->stop();
            
            break;
        case TFMediaPlayerStatePause:
            
            break;
            
        default:
            break;
    }
    
    [_audioPlayer stop];
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
    [_audioPlayer setShareAudioStruct:shareAudioStruct];
}

#pragma mark - platform special

int displayVideoFrame_iOS(TFMPVideoFrameBuffer *frameBuf, void *context){
    TFMediaPlayer *player = (__bridge TFMediaPlayer *)context;
    
    [player->_displayView displayFrameBuffer:frameBuf];
    
    return 0;
}



@end
