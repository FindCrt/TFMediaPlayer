//
//  TFNetMp4PlayViewController.m
//  TFMediaPlayer
//
//  Created by shiwei on 17/12/25.
//  Copyright © 2017年 shiwei. All rights reserved.
//

#import "TFNetMp4PlayViewController.h"
#import "TFMediaPlayer.h"
#import <AVFoundation/AVFoundation.h>


@interface TFNetMp4PlayViewController ()<UITextFieldDelegate, UITableViewDelegate, UITableViewDataSource>{
    UITextField *_urlInputView;
    
    TFMediaPlayer *_player;
    
    NSMutableArray *_medias;
    
    NSURL *_curURL;
    
    UIButton *_playButton;
    
    UITableView *_tableView;
}

@end

@implementation TFNetMp4PlayViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    self.edgesForExtendedLayout = UIRectEdgeNone;
    self.view.backgroundColor = [UIColor whiteColor];
    
    self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc] initWithTitle:@"列表" style:(UIBarButtonItemStylePlain) target:self action:@selector(clickMediaButton)];
    
    NSString *mediaConfig = [[NSBundle mainBundle] pathForResource:@"netMedias" ofType:@"plist"];
    _medias = [[NSMutableArray alloc] initWithContentsOfFile:mediaConfig];
    NSLog(@"_medias: %@",_medias);;
    
    
    CGFloat margin = 20;
    _urlInputView = [[UITextField alloc] initWithFrame:CGRectMake(20, 20, [UIScreen mainScreen].bounds.size.width - margin*2, 40)];
    _urlInputView.borderStyle = UITextBorderStyleLine;
    _urlInputView.returnKeyType = UIReturnKeyGo;
    _urlInputView.delegate = self;
    _urlInputView.placeholder = @"输入音视频网络地址";
    [self.view addSubview:_urlInputView];
    

    _player = [[TFMediaPlayer alloc] initWithParams:@{@"activeVTB":@(YES)}];
    _player.mediaType = TFMP_MEDIA_TYPE_ALL_AVIABLE;
    _player.displayView.frame = CGRectMake(0, 100, [UIScreen mainScreen].bounds.size.width, 240);
    _player.displayView.contentMode = UIViewContentModeScaleAspectFit;
    _player.displayView.backgroundColor = [UIColor blackColor];
    [self.view addSubview:_player.displayView];
    
    _playButton = [[UIButton alloc] initWithFrame:CGRectMake(0, 0, 100, 60)];
    _playButton.center = CGPointMake([UIScreen mainScreen].bounds.size.width/2.0, CGRectGetMaxY(_player.displayView.frame)+50);
    _playButton.layer.cornerRadius = 5;
    _playButton.backgroundColor = [UIColor orangeColor];
    [_playButton setTitle:@"播放" forState:(UIControlStateNormal)];
    [_playButton setTitleColor:[UIColor whiteColor] forState:(UIControlStateNormal)];
    [_playButton addTarget:self action:@selector(clickPlayButton) forControlEvents:(UIControlEventTouchUpInside)];
    [_playButton setTitle:@"停止" forState:(UIControlStateSelected)];
//    [self.view addSubview:_playButton];
    
    _tableView = [[UITableView alloc] initWithFrame:self.view.bounds style:(UITableViewStyleGrouped)];
    _tableView.delegate = self;
    _tableView.dataSource = self;
    _tableView.layer.anchorPoint = CGPointMake(1, 0);
    _tableView.frame = self.view.bounds;
    _tableView.hidden = YES;
    [self.view addSubview:_tableView];
}

-(void)viewDidLayoutSubviews{
    [super viewDidLayoutSubviews];
    
    if ([UIDevice currentDevice].orientation == UIDeviceOrientationPortrait || [UIDevice currentDevice].orientation == UIDeviceOrientationPortraitUpsideDown) {
        _player.displayView.frame = CGRectMake(0, 100, [UIScreen mainScreen].bounds.size.width, 240);
    }else{
        _player.displayView.frame = self.view.bounds;
    }
    
    _tableView.frame = self.view.bounds;
}

-(void)viewWillDisappear:(BOOL)animated{
    [super viewWillDisappear:animated];
    
    [_player stop];
}

-(BOOL)textFieldShouldReturn:(UITextField *)textField{
    _curURL = [NSURL URLWithString:textField.text];
    
    [self startPlay];
    
    return YES;
}



-(void)clickPlayButton{
    _playButton.selected = !_playButton.selected;
    
    if (_playButton.selected) {
        _playButton.backgroundColor = [UIColor colorWithRed:0.3 green:0.3 blue:1 alpha:1];
        [self startPlay];
    }else{
        _playButton.backgroundColor = [UIColor orangeColor];
        [self stop];
    }
}

-(void)startPlay{
    if ([self configureAVSession]) {
        _player.mediaURL = _curURL;
        [_player play];
    }
}

-(void)stop{
    [_player stop];
}

-(BOOL)configureAVSession{
    
    NSError *error = nil;
    
    [[AVAudioSession sharedInstance]setCategory:AVAudioSessionCategoryPlayback withOptions:AVAudioSessionCategoryOptionDuckOthers error:&error];
    if (error) {
        //        TFMPDLog(@"audio session set category error: %@",error);
        return NO;
    }
    
    [[AVAudioSession sharedInstance] setActive:YES error:&error];
    if (error) {
        //        TFMPDLog(@"active audio session error: %@",error);
        return NO;
    }
    
    return YES;
}

-(void)clickMediaButton{
    if (_tableView.hidden) {
        [self showMediaLists];
    }else{
        [self hideMediaList];
    }
}

-(void)showMediaLists{
    
    _tableView.hidden = NO;
    _tableView.transform = CGAffineTransformMakeScale(0.1, 0.1);
    
    [UIView animateWithDuration:0.25f animations:^{
        _tableView.transform = CGAffineTransformIdentity;
    } completion:^(BOOL finished) {
        
    }];
}

-(void)hideMediaList{
    
    [UIView animateWithDuration:0.25 animations:^{
        _tableView.transform = CGAffineTransformMakeScale(0.1, 0.1);
    } completion:^(BOOL finished) {
        _tableView.hidden = YES;
    }];
}

#pragma mark - tableView delegate & data source

-(NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section{
    return _medias.count;
}

-(UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath{
    UITableViewCell *cell = [[UITableViewCell alloc] initWithStyle:(UITableViewCellStyleDefault) reuseIdentifier:nil];
    
    NSDictionary *mediaInfo = _medias[indexPath.row];
    cell.textLabel.text = [mediaInfo objectForKey:@"name"];
    
    cell.selectionStyle = UITableViewCellSelectionStyleNone;
    return cell;
}

-(void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath{
    NSDictionary *mediaInfo = _medias[indexPath.row];
    
    NSString *url = [mediaInfo objectForKey:@"url"];
    if ([url hasPrefix:@"local://"]) {
        NSString *extension = [url pathExtension];
        NSString *name = [[url substringFromIndex:8] stringByDeletingPathExtension];
        _curURL = [[NSBundle mainBundle] URLForResource:name withExtension:extension];
    }else{
        _curURL = [NSURL URLWithString:url];
    }
    
    [self hideMediaList];
    
    [self configureAVSession];
    [_player switchToNewMedia:_curURL];
}

-(void)dealloc{
    NSLog(@"%@ dealloc",[self class]);
}

@end
