//
//  TFAudioUnitPlayer.m
//  TFMediaPlayer
//
//  Created by shiwei on 18/1/8.
//  Copyright © 2018年 shiwei. All rights reserved.
//

#import "TFAudioUnitPlayer.h"
#import <AudioUnit/AudioUnit.h>
#include <AVFoundation/AVFoundation.h>
#import "TFMPDebugFuncs.h"

static UInt32 renderAudioElement = 0;//the id of element that render to system audio component.

@interface TFAudioUnitPlayer (){
    AudioUnit audioUnit;
    
    TFMPAudioStreamDescription tfmpResultDesc;
    
    AudioStreamBasicDescription audioUnitResultDesc;
}

@end

@implementation TFAudioUnitPlayer

-(instancetype)init{
    if (self = [super init]) {
        
    }
    
    return self;
}

-(TFMPAudioStreamDescription)resultAudioDescForSource:(TFMPAudioStreamDescription)sourceDesc{
    
    //all return s16+44100,but don't change channel number.
    tfmpResultDesc.sampleRate = 44100;
    setFormatFlagsForAudioDesc(&tfmpResultDesc, true, true, isBigEndianForAudioDesc(sourceDesc.formatFlags));
    tfmpResultDesc.bitsPerChannel = 16;
    tfmpResultDesc.channelsPerFrame = sourceDesc.channelsPerFrame;
    
    tfmpResultDesc.ffmpeg_channel_layout = sourceDesc.ffmpeg_channel_layout;
    
    [self prepareAudioUnit];
    
    return tfmpResultDesc;
}

-(void)prepareAudioUnit{
    
    //gen Audio Unit audio description from tfmpResultDesc
    
    audioUnitResultDesc.mSampleRate = tfmpResultDesc.sampleRate;
    audioUnitResultDesc.mFormatID = kAudioFormatLinearPCM;
    
    audioUnitResultDesc.mFormatFlags = 0; //reset.
    if (isIntForAudioDesc(tfmpResultDesc.formatFlags)) {
        if (isSignedForAudioDesc(tfmpResultDesc.formatFlags)) {
            audioUnitResultDesc.mFormatFlags |= kAudioFormatFlagIsSignedInteger;
        }
    }else{
        audioUnitResultDesc.mFormatFlags |= kAudioFormatFlagIsFloat;
    }
    if (isBigEndianForAudioDesc(tfmpResultDesc.formatFlags)) {
        audioUnitResultDesc.mFormatFlags |= kAudioFormatFlagIsBigEndian;
    }
    
    audioUnitResultDesc.mBitsPerChannel = tfmpResultDesc.bitsPerChannel;
    audioUnitResultDesc.mChannelsPerFrame = tfmpResultDesc.channelsPerFrame;
    audioUnitResultDesc.mBytesPerFrame = audioUnitResultDesc.mBitsPerChannel * audioUnitResultDesc.mChannelsPerFrame / 8;
    audioUnitResultDesc.mBytesPerPacket = audioUnitResultDesc.mBytesPerFrame;
    audioUnitResultDesc.mFramesPerPacket = 1;
    
    //setup audio unit
    [self setupAudioUnitRenderWithAudioDesc:audioUnitResultDesc];
}

-(BOOL)play{
    
    NSAssert(_fillStruct.fillFunc, @"fillFunc is nil");
    
    if (AudioOutputUnitStart(audioUnit) != 0) {
        TFMPDLog(@"start audio player failed!");
        
        return NO;
    }
    
    return YES;
}

-(void)pause{
    AudioOutputUnitStop(audioUnit);
}

-(void)stop{
    AudioOutputUnitStop(audioUnit);
    
    //release resources
    AudioComponentInstanceDispose(audioUnit);
}

-(void)setupAudioUnitRenderWithAudioDesc:(AudioStreamBasicDescription)audioDesc{
    
    AudioComponentInstanceDispose(audioUnit); //dispose previous audioUnit
    
    //componentDesc是筛选条件 component是组件的抽象，对应class的角色，componentInstance是具体的组件实体，对应object角色。
    AudioComponentDescription componentDesc;
    componentDesc.componentType = kAudioUnitType_Output;
    componentDesc.componentSubType = kAudioUnitSubType_RemoteIO;
    componentDesc.componentFlags = 0;
    componentDesc.componentFlagsMask = 0;
    componentDesc.componentManufacturer = kAudioUnitManufacturer_Apple;
    
    AudioComponent component = AudioComponentFindNext(NULL, &componentDesc);
    OSStatus status = AudioComponentInstanceNew(component, &audioUnit);
    
    TFCheckStatusUnReturn(status, @"instance new audio component");
    
    //open render output
    UInt32 falg = 1;
    status = AudioUnitSetProperty(audioUnit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output, renderAudioElement, &falg, sizeof(UInt32));
    
    TFCheckStatusUnReturn(status, @"enable IO");
    
    //set render input format
    status = AudioUnitSetProperty(audioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, renderAudioElement, &audioDesc, sizeof(audioDesc));
    
    TFCheckStatusUnReturn(status, @"set render input format");
    
    //set render callback to process audio buffers
    AURenderCallbackStruct callbackSt;
    callbackSt.inputProcRefCon = (__bridge void * _Nullable)(self);
    callbackSt.inputProc = playAudioBufferCallback;
    status = AudioUnitSetProperty(audioUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Group, renderAudioElement, &callbackSt, sizeof(callbackSt));
    
    TFCheckStatusUnReturn(status, @"set render callback");
}


#pragma mark - callback

OSStatus playAudioBufferCallback(	void *							inRefCon,
                                 AudioUnitRenderActionFlags *	ioActionFlags,
                                 const AudioTimeStamp *			inTimeStamp,
                                 UInt32							inBusNumber,
                                 UInt32							inNumberFrames,
                                 AudioBufferList * __nullable	ioData){
    
    TFAudioUnitPlayer *player = (__bridge TFAudioUnitPlayer *)(inRefCon);
    
//    UInt32 framesPerPacket = inNumberFrames;
    
//    int size = 0;
    
    //TODO: 转换成中间格式的大小。
    int retval = player->_fillStruct.fillFunc(ioData->mBuffers[0].mData, ioData->mBuffers[0].mDataByteSize, player->_fillStruct.context);
    
    return retval;
}

@end
