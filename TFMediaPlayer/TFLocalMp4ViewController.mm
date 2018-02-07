//
//  TFLocalMp4ViewController.m
//  TFMediaPlayer
//
//  Created by shiwei on 17/12/28.
//  Copyright © 2017年 shiwei. All rights reserved.
//

#import "TFLocalMp4ViewController.h"
#import "TFMediaPlayer.h"
#import <AVFoundation/AVFoundation.h>

#import "TFAudioFileReader.h"
#import "TFAudioUnitPlayer.h"
#import "TFMPAVFormat.h"
#import "TFMPDebugFuncs.h"
#import "TFAudioPowerGraphView.h"

@interface TFLocalMp4ViewController (){
    TFMediaPlayer *_player;
    
    TFAudioFileReader *_reader;
    TFAudioUnitPlayer *_audioPlayer;
    
    TFAudioPowerGraphView *_graphView;
    
    UIButton *_stopButton;
    UIButton *_mediaTypeButton;
    UIButton *_switchMediaSourceBtn;
    UIButton *_switchContentModeBtn;
    
    UIButton *_changeFrameBtn;
    
    BOOL _showGraph;
    BOOL _autoStop;
    
    BOOL _playing;
    
    NSURL *_currentMediaURL;
}

@end

@implementation TFLocalMp4ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    
    self.automaticallyAdjustsScrollViewInsets = NO;
    self.edgesForExtendedLayout = UIRectEdgeNone;
    
    self.view.backgroundColor = [UIColor whiteColor];
    
//    _showGraph = YES;
//    _autoStop = YES;
    
    
    
    [self setupButtons];
    
    if (_showGraph) {
        _graphView = [[TFAudioPowerGraphView alloc] initWithFrame:CGRectMake(0, 100, [UIScreen mainScreen].bounds.size.width, 200)];
        [self.view addSubview:_graphView];
        
        _graphView.sampleRate = 44100;
        _graphView.bytesPerSample = 2;
//        _graphView.ignoreSign = YES;
        _graphView.showRate = 1;
        _graphView.changeColor = YES;
        _graphView.changeBGColor = YES;
//        _graphView.colorFlagCycleCount = 2;
    }
    
    _player = [[TFMediaPlayer alloc] init];
    _player.mediaType = TFMP_MEDIA_TYPE_ALL_AVIABLE;
    _player.displayView.frame = CGRectMake(0, _showGraph ? CGRectGetMaxY(_graphView.frame) : 100, [UIScreen mainScreen].bounds.size.width, 300);
    _player.displayView.contentMode = UIViewContentModeScaleToFill;
    if (_showGraph) {
        _player.shareAudioStruct = {shareAudioBuffer, (__bridge void*)self};
    }
    [self.view addSubview:_player.displayView];
    
    _changeFrameBtn = [[UIButton alloc] initWithFrame:CGRectMake(100, 540, 60, 40)];
    [_changeFrameBtn setTitle:@"change" forState:(UIControlStateNormal)];
    _changeFrameBtn.backgroundColor = [UIColor orangeColor];
    [_changeFrameBtn addTarget:self action:@selector(changeFrame:) forControlEvents:(UIControlEventTouchUpInside)];
    [self.view addSubview:_changeFrameBtn];
}

-(void)viewWillDisappear:(BOOL)animated{
    [super viewWillDisappear:animated];
    dispatch_async(dispatch_get_global_queue(0, 0), ^{
        [_player stop];
        
        [_audioPlayer stop];
        [_graphView stop];
    });
    
    NSLog(@"disappeared！");
}

-(void)changeFrame:(UIButton *)button{
    CGRect frame = _player.displayView.frame;
    frame.size.height += 20;
    _player.displayView.frame = frame;
}

#pragma mark - actions

-(void)setupButtons{
    _stopButton = [[UIButton alloc] initWithFrame:CGRectMake(10, 20, 60, 40)];
    _stopButton.backgroundColor = [UIColor orangeColor];
    [_stopButton setTitle:@"play" forState:(UIControlStateNormal)];
    [_stopButton addTarget:self action:@selector(stopAndPlay:) forControlEvents:(UIControlEventTouchUpInside)];
    [self.view addSubview:_stopButton];
    
    _mediaTypeButton = [[UIButton alloc] initWithFrame:CGRectMake(CGRectGetMaxX(_stopButton.frame)+10, CGRectGetMinY(_stopButton.frame), 100, 40)];
    _mediaTypeButton.backgroundColor = [UIColor orangeColor];
    [_mediaTypeButton setTitle:@"M-Type" forState:(UIControlStateNormal)];
    [_mediaTypeButton addTarget:self action:@selector(changeMediaType:) forControlEvents:(UIControlEventTouchUpInside)];
    [self.view addSubview:_mediaTypeButton];
    
    _switchMediaSourceBtn = [[UIButton alloc] initWithFrame:CGRectMake(CGRectGetMaxX(_mediaTypeButton.frame)+10, CGRectGetMinY(_stopButton.frame), 60, 40)];
    _switchMediaSourceBtn.backgroundColor = [UIColor orangeColor];
    [_switchMediaSourceBtn setTitle:@"source" forState:(UIControlStateNormal)];
    [_switchMediaSourceBtn addTarget:self action:@selector(switchMediaSource) forControlEvents:(UIControlEventTouchUpInside)];
    [self.view addSubview:_switchMediaSourceBtn];
    
    _switchContentModeBtn = [[UIButton alloc] initWithFrame:CGRectMake(CGRectGetMaxX(_switchMediaSourceBtn.frame)+10, CGRectGetMinY(_stopButton.frame), 100, 40)];
    _switchContentModeBtn.backgroundColor = [UIColor orangeColor];
    [_switchContentModeBtn setTitle:@"C-mode" forState:(UIControlStateNormal)];
    [_switchContentModeBtn addTarget:self action:@selector(switchContentMode) forControlEvents:(UIControlEventTouchUpInside)];
    [self.view addSubview:_switchContentModeBtn];
}

-(void)stopAndPlay:(UIButton *)button{
    
    if (_playing) {
        [_audioPlayer stop];
        [_graphView stop];
        [_player stop];
        
        [button setTitle:@"play" forState:(UIControlStateNormal)];
    }else{
        [self startPlay];
        [button setTitle:@"stop" forState:(UIControlStateNormal)];
    }
    
    _playing = !_playing;
}

-(void)changeMediaType:(UIButton *)button{
    TFMPMediaType mediaType = _player.mediaType;
    if (mediaType == TFMP_MEDIA_TYPE_AUDIO) {
        _player.mediaType = TFMP_MEDIA_TYPE_VIDEO;
    }else if (mediaType == TFMP_MEDIA_TYPE_VIDEO){
        _player.mediaType = TFMP_MEDIA_TYPE_ALL_AVIABLE;
    }else if (mediaType == TFMP_MEDIA_TYPE_ALL_AVIABLE){
        _player.mediaType = TFMP_MEDIA_TYPE_AUDIO;
    }
}

static int mediaIndex = 0;
-(void)switchMediaSource{
    
    NSURL *videoURL1 = [[NSBundle mainBundle] URLForResource:@"game" withExtension:@"mp4"];
    NSURL *videoURL2 = [[NSBundle mainBundle] URLForResource:@"cocosvideo" withExtension:@"mp4"];
    NSURL *videoURL3 = [[NSBundle mainBundle] URLForResource:@"AACTest" withExtension:@"m4a"];

    NSURL *videoURL4 = [[NSBundle mainBundle] URLForResource:@"LuckyDay" withExtension:@"mp3"];

    NSURL *videoURL5 = [[NSBundle mainBundle] URLForResource:@"pure1" withExtension:@"caf"];
    
    NSArray *mediaSources = @[videoURL1,videoURL2,videoURL5];
    
    _currentMediaURL = mediaSources[mediaIndex];
    mediaIndex++;
    if (mediaIndex >= mediaSources.count) {
        mediaIndex = 0;
    }
    
    [self stop];
    [self startPlay];
}

-(void)switchContentMode{
    NSInteger contentMode = _player.displayView.contentMode;
    if (contentMode == UIViewContentModeBottomRight) {
        contentMode = -1;
    }
    _player.displayView.contentMode = (UIViewContentMode)(contentMode + 1);
    NSLog(@"contentMode %ld",_player.displayView.contentMode);
}

-(void)stop{
    [_player stop];
    [_audioPlayer stop];
    
    if (_graphView) [_graphView stop];
}

-(void)startPlay{
    
    if (!_currentMediaURL) {
//        _currentMediaURL = [[NSBundle mainBundle] URLForResource:@"game" withExtension:@"mp4"];
//        _currentMediaURL = [NSURL URLWithString:@"http://oys9374yl.bkt.clouddn.com/voice/278/15d8a04d-15eb-4d12-809b-c9be924ad92f.mp3"];
        _currentMediaURL = [NSURL URLWithString:@"http://flv2.bn.netease.com/videolib3/1711/04/aMSoJ8166/HD/aMSoJ8166-mobile.mp4"];
    }
    
    _player.mediaURL = _currentMediaURL;
    [self configureAVSession];
    
    
    [_player play];
//    [self audioUnitPlay];
    
    
    if (_autoStop) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            [_graphView stop];
            [_player stop];
            
            [_audioPlayer stop];
        });
    }
}


-(BOOL)configureAVSession{
    
    NSError *error = nil;
    
    [[AVAudioSession sharedInstance]setCategory:AVAudioSessionCategoryPlayback withOptions:AVAudioSessionCategoryOptionMixWithOthers error:&error];
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

#pragma mark - system decode + audio unit player

int fillAudioBuffer(uint8_t **buffer, int lineCount, int oneLineize,void *context){
    TFLocalMp4ViewController *localPlayer = (__bridge TFLocalMp4ViewController *)context;
    
    int bytesPerFrame = localPlayer->_reader.outputDesc.mBytesPerFrame;
    uint32_t framesNum = oneLineize/bytesPerFrame;
    
    AudioBuffer audioBuffer = {localPlayer->_reader.outputDesc.mChannelsPerFrame, (UInt32)(oneLineize), buffer[0]};
    AudioBufferList bufList;
    bufList.mNumberBuffers = 1;
    bufList.mBuffers[0] = audioBuffer;
    
    TFAudioBufferData *tfBufData = TFCreateAudioBufferData(&bufList, framesNum);
    
    int status = [localPlayer->_reader readFrames:&framesNum toBufferData:tfBufData];
    
    if (localPlayer->_showGraph && buffer) {
        [localPlayer->_graphView showBuffer:buffer[0] size:oneLineize];
    }
    
    return status;
}



-(void)audioUnitPlay{
    _reader = [[TFAudioFileReader alloc] init];
    
    NSString *audioPath = [[NSBundle mainBundle] pathForResource:@"LuckyDay" ofType:@"mp3"];
//    NSString *audioPath = [[NSBundle mainBundle] pathForResource:@"pure1" ofType:@"caf"];
    [_reader setFilePath:audioPath];
    _reader.isRepeat = true;
    
    _audioPlayer = [[TFAudioUnitPlayer alloc] init];
    
    TFMPAudioStreamDescription tfmpAudioDesc;
    tfmpAudioDesc.bitsPerChannel = _reader.outputDesc.mSampleRate;
    tfmpAudioDesc.channelsPerFrame = _reader.outputDesc.mChannelsPerFrame;
    tfmpAudioDesc.sampleRate = _reader.outputDesc.mSampleRate;
    setFormatFlagsWithFlags(&tfmpAudioDesc.formatFlags, true, true, false, false);
    
    [_audioPlayer resultAudioDescForSource:tfmpAudioDesc];
    
    TFMPFillAudioBufferStruct fullStruct = {fillAudioBuffer, (__bridge void *)(self)};
    [_audioPlayer setFillStruct:fullStruct];
    
    [_audioPlayer play];
    
    [_graphView start];
}

#pragma mark - ffmpeg decode + audio unit player

void shareAudioBuffer(uint8_t **buffer, int size, void *context){
    
    TFLocalMp4ViewController *localPlayer = (__bridge TFLocalMp4ViewController *)context;
    
    if (buffer) [localPlayer->_graphView showBuffer:buffer[0] size:size];
}

@end
