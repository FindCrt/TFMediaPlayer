//
//  TFAudioQueueController.m
//  TFLive
//
//  Created by wei shi on 2017/7/4.
//  Copyright © 2017年 wei shi. All rights reserved.
//

#import "TFAudioQueueController.h"
#import <AudioToolbox/AudioToolbox.h>
#import <AVFoundation/AVFoundation.h>
#import "TFMPUtilities.h"


@interface TFAudioQueueController (){
    
    AudioQueueRef _audioQueue;
    AudioQueueBufferRef _audioBufferArray[TFAudioQueueBufferCount];
    
    BOOL _isPaused;
    BOOL _isStoped;
    NSLock *_lock;
}

@end

@implementation TFAudioQueueController

-(instancetype)initWithSpecifics:(const TFMPAudioStreamDescription)specifics{
    if (self = [super init]) {

        [self resultAudioDescForSource:specifics];
        
        AudioStreamBasicDescription audioDesc;
        configAudioDescWithSpecifics(&audioDesc, &_resultSpecifics);
        
        _resultSpecifics.samples = 1024;
        _resultSpecifics.bufferSize = _resultSpecifics.bitsPerChannel/8 * _resultSpecifics.channelsPerFrame * _resultSpecifics.samples;
        
        
        AudioQueueNewOutput(&audioDesc, TFAudioQueueHasEmptyBufferCallBack, (__bridge void*)self, NULL, (__bridge CFStringRef)NSRunLoopCommonModes, 0, &(_audioQueue));
        
        OSStatus status = AudioQueueStart(_audioQueue, NULL);
        if (status != noErr) {
            NSLog(@"audio queue start error");
            
            self = nil;
            return nil;
        }
        
        for (int i = 0; i<TFAudioQueueBufferCount; i++) {
            AudioQueueAllocateBuffer(_audioQueue, _resultSpecifics.bufferSize, &_audioBufferArray[i]);
            _audioBufferArray[i]->mAudioDataByteSize = _resultSpecifics.bufferSize;
            memset(_audioBufferArray[i]->mAudioData, 0, _resultSpecifics.bufferSize);
            AudioQueueEnqueueBuffer(_audioQueue, _audioBufferArray[i], 0, NULL);
        }
        
        _isStoped = NO;
        _isPaused = NO;
        _lock = [[NSLock alloc] init];
    }
    
    return self;
}

-(void)resultAudioDescForSource:(TFMPAudioStreamDescription)sourceDesc{
    
    //all return s16+44100(no planar),but don't change channel number.
    _resultSpecifics.samples = sourceDesc.samples;
    _resultSpecifics.sampleRate = 48000;
    setFormatFlagsWithFlags(&(_resultSpecifics.formatFlags),
                            true,
                            true,
                            isBigEndianForFormatFlags(sourceDesc.formatFlags),
                            false);
    
    _resultSpecifics.bitsPerChannel = 16;
    _resultSpecifics.channelsPerFrame = sourceDesc.channelsPerFrame;
    
    _resultSpecifics.ffmpeg_channel_layout = channelLayoutForChannels(_resultSpecifics.channelsPerFrame);
    
    //    tfmpResultDesc = sourceDesc
}

static void configAudioDescWithSpecifics(AudioStreamBasicDescription *audioDesc, TFMPAudioStreamDescription *specifics){
    
    audioDesc->mSampleRate = specifics->sampleRate;
    audioDesc->mFormatID = kAudioFormatLinearPCM;
    audioDesc->mFormatFlags = kLinearPCMFormatFlagIsPacked;
    audioDesc->mFramesPerPacket = 1;
    audioDesc->mChannelsPerFrame = specifics->channelsPerFrame;
    
    audioDesc->mBitsPerChannel = specifics->bitsPerChannel;
    if (isBigEndianForFormatFlags(specifics->formatFlags))
        audioDesc->mFormatFlags |= kLinearPCMFormatFlagIsBigEndian;
    if (!isIntForFormatFlags(specifics->formatFlags))
        audioDesc->mFormatFlags |= kLinearPCMFormatFlagIsFloat;
    if (isSignedForFormatFlags(specifics->formatFlags))
        audioDesc->mFormatFlags |= kLinearPCMFormatFlagIsSignedInteger;
    
    audioDesc->mBytesPerFrame = audioDesc->mBitsPerChannel * audioDesc->mChannelsPerFrame / 8;
    audioDesc->mBytesPerPacket = audioDesc->mBytesPerFrame * audioDesc->mFramesPerPacket;
}

-(void)play{
    if (!_audioQueue) {
        return;
    }
    
    [_lock lock];
    
    if (!_isPaused && !_isStoped) {
        [_lock unlock];
        return;
    }
    
    NSError *error;
    
    if ([[AVAudioSession sharedInstance] setActive:YES error:&error]) {
        NSLog(@"audio session active error: %@",error);
        [_lock unlock];
        return;
    }
    
    OSStatus status = AudioQueueStart(_audioQueue, NULL);
    if (status != noErr) {
        NSLog(@"audio queue start error");
        [_lock unlock];
        return;
    }
    
    _isPaused = NO;
    _isStoped = NO;
    
    
    
    [_lock unlock];
}

-(void)pause{
    [_lock lock];
    
    if (!_isPaused) {
        [_lock unlock];
        return;
    }
    
    AudioQueuePause(_audioQueue);
    _isPaused = YES;
    
    [_lock unlock];
}

-(void)stop{
    [_lock lock];
    
    if (_isStoped) {
        [_lock unlock];
        return;
    }
    
    AudioQueueStop(_audioQueue, YES);
    _isStoped = YES;
    
    [_lock unlock];
}

static void TFAudioQueueHasEmptyBufferCallBack(void *inUserData, AudioQueueRef inAQ, AudioQueueBufferRef inBuffer){
    TFAudioQueueController *controller = (__bridge TFAudioQueueController *)(inUserData);
    
    if (!controller) {
        return;
    }
    if (controller->_isStoped || controller->_isPaused) {
        return;
    }
    
    int count = 1;
    uint8_t **buffers = (uint8_t**)malloc(count*sizeof(uint8_t*));
    buffers[0] = (uint8_t*)inBuffer->mAudioData;
    
    if (controller.fillStruct.fillFunc) {
        int retval = controller.fillStruct.fillFunc(buffers,count, inBuffer->mAudioDataByteSize, controller.fillStruct.context);
        if (retval == -1) {
            return;
        }
        
        AudioQueueEnqueueBuffer(inAQ, inBuffer, 0, NULL);
    }
}

-(void)dealloc{
    NSLog(@"AUDIO QUEUE DEALLOCED");
}

@end
