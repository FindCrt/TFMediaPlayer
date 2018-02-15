//
//  TFMPPlayControlView.h
//  TFMediaPlayer
//
//  Created by shiwei on 2018/2/15.
//  Copyright © 2018年 shiwei. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "TFMPControlProtocol.h"

@interface TFMPPlayControlView : UIView

@property (nonatomic, weak) id<TFMPControlProtocol> delegate;



@end
