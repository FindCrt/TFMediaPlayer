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
        
        _playController->displayContext = (__bridge void *)self;
        _playController->displayVideoFrame = displayVideoFrame_iOS;
        
        _playController->connectCompleted = [](tfmpcore::PlayController *playController){
            
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
    _playController->connectAndOpenMedia([[_mediaURL absoluteString] cStringUsingEncoding:NSUTF8StringEncoding]);
}

-(void)play{
    if (_state == TFMediaPlayerStateNone) {
        
        [self preparePlay];
        _innerPlayWhenReady = YES;
        
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
            
            break;
        case TFMediaPlayerStateReady:
            
            break;
        case TFMediaPlayerStatePlaying:
            
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
