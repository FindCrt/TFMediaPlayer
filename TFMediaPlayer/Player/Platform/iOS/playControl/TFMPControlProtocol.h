//
//  TFMPControlProtocol.h
//  TFMediaPlayer
//
//  Created by shiwei on 2018/2/15.
//  Copyright © 2018年 shiwei. All rights reserved.
//

#import <Foundation/Foundation.h>

typedef void(^TFMPStateObserveHandler) (NSString *state, id value);

@protocol TFMPControlProtocol <NSObject>

-(void)dealPlayControlCommand:(NSString *)command params:(NSDictionary<NSString *, id> *)params;

-(void)helpObserveStates:(NSArray<NSString *> *)states withChangedHandler:(TFMPStateObserveHandler)handler;

@end


/** commands **/

static NSString *TFMPCmd_play = @"TFMPCmd_play";
static NSString *TFMPCmd_pause = @"TFMPCmd_pause";
static NSString *TFMPCmd_stop = @"TFMPCmd_stop";

/** seek to one time point */
static NSString *TFMPCmd_seek_TP = @"TFMPCmd_seek_TP";

/**
 * seek to a time by add or minus a duration.
 * params: {@"duration" : @(10)}, unit is second.
 */
static NSString *TFMPCmd_seek_TD = @"TFMPCmd_seek_TD";

static NSString *TFMPCmd_fullScreen = @"TFMPCmd_fullScreen";
static NSString *TFMPCmd_volume = @"TFMPCmd_volume";
static NSString *TFMPCmd_rate = @"TFMPCmd_rate";
static NSString *TFMPCmd_allowMedias = @"TFMPCmd_allowMedias";
static NSString *TFMPCmd_brightness = @"TFMPCmd_brightness";



/** params **/

static NSString *TFMPCmd_param_duration = @"duration";

static NSString *TFMPCmd_param_time = @"time";


/** states **/

static NSString *TFMPState_duration = @"duration";

static NSString *TFMPState_currentTime = @"TFMPState_currentTime";
