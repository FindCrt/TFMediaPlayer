//
//  TFMPPlayCmdResolver.h
//  TFMediaPlayer
//
//  Created by shiwei on 2018/2/15.
//  Copyright © 2018年 shiwei. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "TFMPControlProtocol.h"

@class TFMediaPlayer;
@interface TFMPPlayCmdResolver : NSObject<TFMPControlProtocol>

@property (nonatomic, weak) TFMediaPlayer *player;

@end
