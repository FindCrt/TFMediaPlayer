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

#import "TFAudioPowerGraphView.h"

@interface TFLocalMp4ViewController (){
    TFMediaPlayer *_player;
    
    TFAudioFileReader *_reader;
    TFAudioUnitPlayer *_audioPlayer;
    
    TFAudioPowerGraphView *_graphView;
}

@end

@implementation TFLocalMp4ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    
    self.automaticallyAdjustsScrollViewInsets = NO;
    self.edgesForExtendedLayout = UIRectEdgeNone;
    
    self.view.backgroundColor = [UIColor whiteColor];
    
    _graphView = [[TFAudioPowerGraphView alloc] initWithFrame:CGRectMake(0, 0, [UIScreen mainScreen].bounds.size.width, 100)];
    [self.view addSubview:_graphView];
    
    _graphView.sampleRate = 44100;
    _graphView.bytesPerSample = 2;
    
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
    
    [_audioPlayer stop];
    [_graphView stop];
}

-(void)startPlay{
    
//    NSURL *videoURL = [[NSBundle mainBundle] URLForResource:@"game" withExtension:@"mp4"];
//    NSURL *videoURL = [[NSBundle mainBundle] URLForResource:@"cocosvideo" withExtension:@"mp4"];
    NSURL *videoURL = [[NSBundle mainBundle] URLForResource:@"AACTest" withExtension:@"m4a"];
    
//    NSURL *videoURL = [[NSBundle mainBundle] URLForResource:@"王崴 - 大城小爱" withExtension:@"mp3"];
    
    _player.mediaURL = videoURL;
    
    [self configureAVSession];
//    [_player play];
    
    [self audioUnitPlay];
}

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
    
    dispatch_async(dispatch_get_main_queue(), ^{
        //graph
        [localPlayer->_graphView showBuffer:buffer[0] size:oneLineize];
    });
    
    return status;
}

-(void)audioUnitPlay{
    _reader = [[TFAudioFileReader alloc] init];
    
    NSString *audioPath = [[NSBundle mainBundle] pathForResource:@"LuckyDay" ofType:@"mp3"];
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

-(BOOL)configureAVSession{
    
    NSError *error = nil;
    
    [[AVAudioSession sharedInstance]setCategory:AVAudioSessionCategoryPlayback error:&error];
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

@end
