//
//  TFAudioQueueController.m
//  TFLive
//
//  Created by wei shi on 2017/7/4.
//  Copyright © 2017年 wei shi. All rights reserved.
//

#import "TFAudioQueueController.h"
#import <AudioToolbox/AudioToolbox.h>
//#import <AVFoundation/AVFoundation.h>

#if DEBUG
#import "TFMPUtilities.h"
#import "TFMPDebugFuncs.h"
#import "TFStateObserver.hpp"
#endif

@interface TFAudioQueueController (){
    
    AudioQueueRef _audioQueue;
    AudioQueueBufferRef _audioBufferArray[TFAudioQueueBufferCount];
    
    NSLock *_lock;
}

@end

@implementation TFAudioQueueController

-(instancetype)initWithSpecifics:(const TFMPAudioStreamDescription)specifics{
    if (self = [super init]) {

        [self resultAudioDescForSource:specifics];
        
        AudioStreamBasicDescription audioDesc;
        configAudioDescWithSpecifics(&audioDesc, &_resultSpecifics);
        
        
        OSStatus status = AudioQueueNewOutput(&audioDesc, TFAudioQueueHasEmptyBufferCallBack, (__bridge void*)self, NULL, (__bridge CFStringRef)NSRunLoopCommonModes, 0, &(_audioQueue));
        TFCheckStatusToDo(status, @"new audioQueue failed!",{
            return nil;
        })
        
        status = AudioQueueStart(_audioQueue, NULL);
        TFCheckStatusToDo(status, @"audio queue start error", {
            return nil;
        })
        
        for (int i = 0; i<TFAudioQueueBufferCount; i++) {
            AudioQueueAllocateBuffer(_audioQueue, _resultSpecifics.bufferSize, &_audioBufferArray[i]);
            _audioBufferArray[i]->mAudioDataByteSize = _resultSpecifics.bufferSize;
            memset(_audioBufferArray[i]->mAudioData, 0, _resultSpecifics.bufferSize);
            AudioQueueEnqueueBuffer(_audioQueue, _audioBufferArray[i], 0, NULL);
        }
        
        AudioQueueAddPropertyListener(_audioQueue, kAudioQueueProperty_IsRunning, audioQueueListen, (__bridge void*)self);
        
        _lock = [[NSLock alloc] init];
    }
    
    return self;
}

-(void)resultAudioDescForSource:(TFMPAudioStreamDescription)sourceDesc{
    
    //all return s16+44100(no planar),but don't change channel number.
    _resultSpecifics.samples = 1024;
    _resultSpecifics.sampleRate = 44100;
    setFormatFlagsWithFlags(&(_resultSpecifics.formatFlags),
                            true,
                            true,
                            isBigEndianForFormatFlags(sourceDesc.formatFlags),
                            false);
    
    _resultSpecifics.bitsPerChannel = 16;
    _resultSpecifics.channelsPerFrame = sourceDesc.channelsPerFrame;
    
    _resultSpecifics.ffmpeg_channel_layout = channelLayoutForChannels(_resultSpecifics.channelsPerFrame);
    
    _resultSpecifics.bufferSize = _resultSpecifics.bitsPerChannel/8 * _resultSpecifics.channelsPerFrame * _resultSpecifics.samples;
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

void audioQueueListen(
                                       void * __nullable       inUserData,
                                       AudioQueueRef           inAQ,
                                       AudioQueuePropertyID    inID){
    
    if (inID == kAudioQueueProperty_IsRunning) {
        UInt32 isRuning = NO;
        UInt32 size = sizeof(isRuning);
        AudioQueueGetProperty(inAQ, kAudioQueueProperty_IsRunning, &isRuning, &size);
        NSLog(@"isRuningxxx: %d",isRuning);
    }
}

-(void)play{
    if (!_audioQueue) {
        return;
    }
    
    [_lock lock];
    
    if (_state == TFAudioQueueStatePlaying) {
        [_lock unlock];
        return;
    }
    
    UInt32 isRuning = NO;
    UInt32 size = sizeof(isRuning);
    AudioQueueGetProperty(_audioQueue, kAudioQueueProperty_IsRunning, &isRuning, &size);
    if (isRuning) {
        AudioQueueFlush(_audioQueue);
    }
    
    OSStatus status = AudioQueueStart(_audioQueue, NULL);
    
    TFCheckStatusToDo(status, @"AudioQueue start failed!", {
        [_lock unlock];
        return;
    })

    _state = TFAudioQueueStatePlaying;
    
    
    [_lock unlock];
}

-(void)pause{
    [_lock lock];
    
    if (_state == TFAudioQueueStatePaused) {
        [_lock unlock];
        return;
    }
    
    OSStatus status = AudioQueuePause(_audioQueue);
    NSLog(@"AudioQueuePause %d\n",status);
    _state = TFAudioQueueStatePaused;
    
    [_lock unlock];
}

-(void)stop{
    [_lock lock];
    
    if (_state == TFAudioQueueStateUnplay) {
        [_lock unlock];
        return;
    }
    
    AudioQueueStop(_audioQueue, YES);
    _state = TFAudioQueueStateUnplay;
    
    [_lock unlock];
}

static void TFAudioQueueHasEmptyBufferCallBack(void *inUserData, AudioQueueRef inAQ, AudioQueueBufferRef inBuffer){
    TFAudioQueueController *controller = (__bridge TFAudioQueueController *)(inUserData);
//    NSLog(@"AudioQueue1\n");
    if (!controller) {
        return;
    }
    if (controller.state != TFAudioQueueStatePlaying) {
        return;
    }
    
    int count = 1;
    uint8_t **buffers = (uint8_t**)malloc(count*sizeof(uint8_t*));
    buffers[0] = (uint8_t*)inBuffer->mAudioData;
    
//    NSLog(@"AudioQueue2\n");
    if (controller.fillStruct.fillFunc) {
        int retval = controller.fillStruct.fillFunc(buffers,count, inBuffer->mAudioDataByteSize, controller.fillStruct.context);
        if (retval < 0) {
            return;
        }
        
        AudioQueueEnqueueBuffer(inAQ, inBuffer, 0, NULL);
        
        
//        TFMPPrintBuffer(buffers[0], 100, 100);
        
        if (controller->_shareAudioStruct.shareAudioFunc) {
            int size = (int)inBuffer->mAudioDataByteSize;
            controller->_shareAudioStruct.shareAudioFunc(buffers, size, controller->_shareAudioStruct.context);
        }
    }
    
    free(buffers);
}

-(void)dealloc{
    NSLog(@"AUDIO QUEUE DEALLOCED");
}

@end
