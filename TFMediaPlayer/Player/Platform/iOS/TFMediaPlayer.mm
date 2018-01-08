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

@interface TFMediaPlayer (){
    tfmpcore::PlayController *_playController;
    
    BOOL _innerPlayWhenReady;
    
    TFOPGLESDisplayView *_displayView;
}

@end

@implementation TFMediaPlayer

-(instancetype)init{
    if (self = [super init]) {
        
        _displayView = [[TFOPGLESDisplayView alloc] init];
        
        _playController = new tfmpcore::PlayController();
        
        _playController->setDisplayMediaType(tfmpcore::TFMP_MEDIA_TYPE_VIDEO);
        _playController->isAudioMajor = false;
        
        _playController->displayContext = (__bridge void *)self;
        _playController->displayVideoFrame = displayVideoFrame_iOS;
        
        _playController->connectCompleted = [self](tfmpcore::PlayController *playController){
            
            if (_state == TFMediaPlayerStateConnecting) {
                _state = TFMediaPlayerStateReady;
            }
            
            if (_innerPlayWhenReady || _autoPlayWhenReady) {
                playController->play();
                _state = TFMediaPlayerStatePlaying;
            }
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
        return;
    }
    
    //local file or bundle file need a file reader which is different on different platform.
    bool succeed = _playController->connectAndOpenMedia([[_mediaURL absoluteString] cStringUsingEncoding:NSUTF8StringEncoding]);
    if (!succeed) {
        NSLog(@"play media open error");
    }
}

-(void)play{
    if (_state == TFMediaPlayerStateNone) {
        
        _innerPlayWhenReady = YES;
        [self preparePlay];
        
    }else if (_state == TFMediaPlayerStateConnecting) {
        _innerPlayWhenReady = YES;
    }else if (_state == TFMediaPlayerStateReady || _state == TFMediaPlayerStatePause){
        _playController->play();
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
}

#pragma mark - platform special

int displayVideoFrame_iOS(TFMPVideoFrameBuffer *frameBuf, void *context){
    TFMediaPlayer *player = (__bridge TFMediaPlayer *)context;
    
    [player->_displayView displayFrameBuffer:frameBuf];
    
    return 0;
}

@end
