//
//  TFMPPlayCmdResolver.m
//  TFMediaPlayer
//
//  Created by shiwei on 2018/2/15.
//  Copyright © 2018年 shiwei. All rights reserved.
//

#import "TFMPPlayCmdResolver.h"
#import "TFMediaPlayer.h"

@implementation TFMPPlayCmdResolver

-(void)dealPlayControlCommand:(NSString *)command params:(NSDictionary<NSString *,id> *)params{
    if (_player == nil) {
        return;
    }
    
    if ([TFMPCmd_play isEqualToString:command]) {
        
    }else if ([TFMPCmd_pause isEqualToString:command]){
        
    }else if ([TFMPCmd_stop isEqualToString:command]){
        
    }else if ([TFMPCmd_seek_TD isEqualToString:command]){
        
        NSTimeInterval duration = [[params objectForKey:TFMPCmd_param_duration] doubleValue];
        [_player seekByForward:duration];
        
    }else if ([TFMPCmd_fullScreen isEqualToString:command]){
        
        [_player changeFullScreenState];
    }
}



@end
