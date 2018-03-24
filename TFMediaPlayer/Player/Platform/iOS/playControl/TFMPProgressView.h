//
//  TFMPProgressView.h
//  TFMediaPlayer
//
//  Created by shiwei on 18/3/3.
//  Copyright © 2018年 shiwei. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface TFMPProgressView : UIView

@property (nonatomic, assign) float duration;

@property (nonatomic, assign) float currentTime;

/** If is't YES, don't notify value changing when user is touching the progressView. */
@property (nonatomic, assign) BOOL notifyWhenUntouch;

@property (nonatomic, copy) void(^valueChangedHandler)(TFMPProgressView *progressView, double touchValue);



@property (nonatomic, strong) UIColor *fillColor;

@property (nonatomic, strong) UIColor *normalColor;


@end
