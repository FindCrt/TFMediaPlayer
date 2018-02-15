//
//  UIDevice+ForceChangeOrientation.m
//  TFMediaPlayer
//
//  Created by shiwei on 2018/2/15.
//  Copyright © 2018年 shiwei. All rights reserved.
//

#import "UIDevice+ForceChangeOrientation.h"

@implementation UIDevice (ForceChangeOrientation)

+(void)changeOrientationTo:(UIDeviceOrientation)orientation{
    SEL selector = NSSelectorFromString(@"setOrientation:");
    NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:[UIDevice instanceMethodSignatureForSelector:selector]];
    [invocation setSelector:selector];
    [invocation setTarget:[UIDevice currentDevice]];
    [invocation setArgument:&orientation atIndex:2];
    [invocation invoke];
}

@end
