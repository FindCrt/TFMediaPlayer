//
//  TFDebugStateShower.m
//  TFMediaPlayer
//
//  Created by shiwei on 2018/9/20.
//  Copyright © 2018年 shiwei. All rights reserved.
//

#import "TFDebugStateShower.h"
#import "TFStateObserver.hpp"

@implementation TFDebugStateShower

static NSTimer *timer;
static UIScrollView *statesView;
bool stateShowing = false;

+(void)startDebuging{
    
    timer = [NSTimer timerWithTimeInterval:0.5 target:self selector:@selector(refresh:) userInfo:nil repeats:YES];
    [[NSRunLoop currentRunLoop] addTimer:timer forMode:NSRunLoopCommonModes];
    
    statesView = [[UIScrollView alloc] init];
    statesView.backgroundColor = [UIColor whiteColor];
    statesView.hidden = YES;
    

    UIButton *showButton = [[UIButton alloc] initWithFrame:CGRectMake(100, 0, 60, 40)];
    showButton.layer.borderColor = [UIColor redColor].CGColor;
    showButton.layer.borderWidth = 1;
    [showButton setTitle:@"states" forState:(UIControlStateNormal)];
    [showButton setTitleColor:[UIColor redColor] forState:(UIControlStateNormal)];
    [showButton setTitleColor:[UIColor colorWithRed:0.4 green:0 blue:0 alpha:1] forState:(UIControlStateSelected)];
    [showButton addTarget:self action:@selector(showHideStates:) forControlEvents:(UIControlEventTouchUpInside)];
    showButton.backgroundColor = [UIColor whiteColor];
    
    [[UIApplication sharedApplication].keyWindow addSubview:showButton];
}

+(void)showHideStates:(UIButton *)button{
    stateShowing = !stateShowing;
    button.selected = !button.selected;
    
    statesView.hidden = !stateShowing;
    
    if (stateShowing) {
        [[UIApplication sharedApplication].keyWindow addSubview:statesView];
    }
}
            
+(void)refresh:(CADisplayLink *)link{
    
    if (statesView.hidden) {
        return;
    }
    
    CGFloat lineHeight = 20, width = [UIScreen mainScreen].bounds.size.width;
    NSInteger count = myStateObserver.getCounts().size() + myStateObserver.getTimeMarks().size() + myStateObserver.getLabels().size();
    
    CGFloat maxHeight = [UIScreen mainScreen].bounds.size.height-40;
    [statesView.subviews makeObjectsPerformSelector:@selector(removeFromSuperview)];
    statesView.frame = CGRectMake(0, 40, width, min(maxHeight, lineHeight*count));
    
    int line = 0;
    for (auto pair : myStateObserver.getCounts()) {
        UILabel *label = [[UILabel alloc] initWithFrame:CGRectMake(0, line*lineHeight, width, lineHeight)];
        label.textColor = [UIColor colorWithWhite:0.1 alpha:1];
        label.font = [UIFont systemFontOfSize:13];
        label.text = [NSString stringWithFormat:@"%s : %d", pair.first.c_str(), pair.second];
        [statesView addSubview:label];
        
        line++;
    }
    
    double currentTime = CACurrentMediaTime();
    for (auto pair : myStateObserver.getTimeMarks()){
        UILabel *label = [[UILabel alloc] initWithFrame:CGRectMake(0, line*lineHeight, width, lineHeight)];
        label.textColor = [UIColor colorWithWhite:0.1 alpha:1];
        label.font = [UIFont systemFontOfSize:13];
        label.text = [NSString stringWithFormat:@"%s : %.3f", pair.first.c_str(), currentTime-pair.second];
        [statesView addSubview:label];
        
        line++;
    }
    
    for (auto pair : myStateObserver.getLabels()){
        UILabel *label = [[UILabel alloc] initWithFrame:CGRectMake(0, line*lineHeight, width, lineHeight)];
        label.textColor = [UIColor colorWithWhite:0.1 alpha:1];
        label.font = [UIFont systemFontOfSize:13];
        label.text = [NSString stringWithFormat:@"%s : %s", pair.first.c_str(), pair.second.c_str()];
        [statesView addSubview:label];
        
        line++;
    }
}

@end
