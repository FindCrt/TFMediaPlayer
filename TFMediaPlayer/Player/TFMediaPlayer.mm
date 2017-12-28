//
//  TFMediaPlayer.m
//  TFMediaPlayer
//
//  Created by shiwei on 17/12/25.
//  Copyright © 2017年 shiwei. All rights reserved.
//

#import "TFMediaPlayer.h"
#import "PlayController.hpp"


@interface TFMediaPlayer (){
    tfmpcore::PlayController *_playController;
    
    BOOL _innerPlayWhenReady;
}

@end

@implementation TFMediaPlayer

-(instancetype)init{
    if (self = [super init]) {
        _playController = new tfmpcore::PlayController();
    }
    
    return self;
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

@end
