//
//  Decoder.cpp
//  TFMediaPlayer
//
//  Created by shiwei on 17/12/28.
//  Copyright © 2017年 shiwei. All rights reserved.
//

#include "Decoder.hpp"
#include "TFMPDebugFuncs.h"
#include "TFMPUtilities.h"
#include <vector>
#include <atomic>
#include <iostream>

#if DEBUG
#include "FFmpegInternalDebug.h"
#endif

using namespace tfmpcore;

void Decoder::startDecode(){
    pthread_create(&decodeThread, NULL, decodeLoopFunc, this);
}

void Decoder::stopDecode(){
    //1. stop decoding
    shouldDecode = false;
    
    //2. disable buffer's IO to prevent decode thread from blocking.
    pktBuffer.disableIO(true);
    frameBuffer.disableIO(true);
    
    //3. wait for the end of decode thread
    pthread_join(decodeThread, nullptr);
    
    //4. flush all reserved buffers
    pktBuffer.flush();
    frameBuffer.flush();
    
    if (codecCtx) avcodec_free_context(&codecCtx);
    
    fmtCtx = nullptr;
}

void Decoder::insertPacket(AVPacket *packet){
    pktBuffer.blockInsert({serial, packet});
}

bool Decoder::isEmpty(){
    return pktBuffer.isEmpty() && frameBuffer.isEmpty();
}


