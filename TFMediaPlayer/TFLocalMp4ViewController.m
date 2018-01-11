//
//  TFLocalMp4ViewController.m
//  TFMediaPlayer
//
//  Created by shiwei on 17/12/28.
//  Copyright © 2017年 shiwei. All rights reserved.
//

#import "TFLocalMp4ViewController.h"
#import "TFMediaPlayer.h"

@interface TFLocalMp4ViewController (){
    TFMediaPlayer *_player;
}

@end

@implementation TFLocalMp4ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    
    self.view.backgroundColor = [UIColor whiteColor];
    
    _player = [[TFMediaPlayer alloc] init];
    _player.displayView.frame = CGRectMake(0, self.view.bounds.size.height/2.0 - 300/2, self.view.bounds.size.width, 300);
    
    [self.view addSubview:_player.displayView];
}

-(void)viewDidAppear:(BOOL)animated{
    [super viewDidAppear:animated];
    
    [self startPlay];
}

-(void)viewWillDisappear:(BOOL)animated{
    [_player stop];
}

-(void)startPlay{
    
    NSURL *videoURL = [[NSBundle mainBundle] URLForResource:@"game" withExtension:@"mp4"];
//    NSURL *videoURL = [[NSBundle mainBundle] URLForResource:@"cocosvideo" withExtension:@"mp4"];
//    NSURL *videoURL = [[NSBundle mainBundle] URLForResource:@"test1" withExtension:@"mp4"];
    
//    NSURL *videoURL = [[NSBundle mainBundle] URLForResource:@"王崴 - 大城小爱" withExtension:@"mp3"];
    
    _player.mediaURL = videoURL;
    
    [_player play];
}

@end
