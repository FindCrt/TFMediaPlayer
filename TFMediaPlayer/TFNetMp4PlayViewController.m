//
//  TFNetMp4PlayViewController.m
//  TFMediaPlayer
//
//  Created by shiwei on 17/12/25.
//  Copyright © 2017年 shiwei. All rights reserved.
//

#import "TFNetMp4PlayViewController.h"
#import "TFMediaPlayer.h"



@interface TFNetMp4PlayViewController ()<UITextFieldDelegate>{
    UITextField *_urlInputView;
    
    TFMediaPlayer *_player;
    
    NSMutableArray *_medias;
    
    NSURL *_curURL;
}

@end

@implementation TFNetMp4PlayViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = [UIColor whiteColor];
    
    _player = [[TFMediaPlayer alloc] init];
    
    CGFloat margin = 20;
    _urlInputView = [[UITextField alloc] initWithFrame:CGRectMake(20, 20, [UIScreen mainScreen].bounds.size.width - margin*2, 40)];
    _urlInputView.borderStyle = UITextBorderStyleLine;
    _urlInputView.returnKeyType = UIReturnKeyGo;
    _urlInputView.delegate = self;
    [self.view addSubview:_urlInputView];
    
    NSString *mediaConfig = [[NSBundle mainBundle] pathForResource:@"netMedias" ofType:@"plist"];
    _medias = [[NSArray alloc] initWithContentsOfFile:mediaConfig];
    
    NSLog(@"_medias: %@",_medias);
}

-(BOOL)textFieldShouldReturn:(UITextField *)textField{
    _curURL = [NSURL URLWithString:textField.text];
    
    
    
    [self startPlay];
    
    return YES;
}

-(void)startPlay{
    _player.mediaURL = [NSURL URLWithString:_curURL];
    
    [_player play];
}

@end
