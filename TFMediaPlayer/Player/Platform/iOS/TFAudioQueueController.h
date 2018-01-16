//
//  TFAudioQueueController.h
//  TFLive
//
//  Created by wei shi on 2017/7/4.
//  Copyright © 2017年 wei shi. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <AudioToolbox/AudioToolbox.h>
#import "TFMPAVFormat.h"
#import "TFAudioUnitPlayer.h"

#define TFAudioQueueBufferCount     3

@interface TFAudioQueueController : NSObject

-(instancetype)initWithSpecifics:(const TFMPAudioStreamDescription)specifics;

@property (nonatomic, assign) TFMPFillAudioBufferStruct fillStruct;

@property (nonatomic, assign) TFMPAudioStreamDescription resultSpecifics;

-(void)play;

-(void)pause;

-(void)stop;

@property (nonatomic, assign) TFMPShareAudioBufferStruct shareAudioStruct;

@end
