//
//  VTBDecoder.cpp
//  TFMediaPlayer
//
//  Created by shiwei on 2018/9/28.
//  Copyright © 2018年 shiwei. All rights reserved.
//

#include "VTBDecoder.hpp"
#include "TFMPUtilities.h"
#include "TFMPDebugFuncs.h"

using namespace tfmpcore;

static void CFDictionarySetSInt32(CFMutableDictionaryRef dictionary, CFStringRef key, SInt32 numberSInt32)
{
    CFNumberRef number;
    number = CFNumberCreate(NULL, kCFNumberSInt32Type, &numberSInt32);
    CFDictionarySetValue(dictionary, key, number);
    CFRelease(number);
}

static void CFDictionarySetBoolean(CFMutableDictionaryRef dictionary, CFStringRef key, bool value)
{
    CFDictionarySetValue(dictionary, key, value ? kCFBooleanTrue : kCFBooleanFalse);
}

void VTBDecoder::decodeCallback(void * CM_NULLABLE decompressionOutputRefCon,void * CM_NULLABLE sourceFrameRefCon,OSStatus status,VTDecodeInfoFlags infoFlags,CM_NULLABLE CVImageBufferRef imageBuffer,CMTime presentationTimeStamp,CMTime presentationDuration ){
    
    
    if (status != 0) {
        return;
    }
    
    VTBDecoder *decoder = (VTBDecoder *)decompressionOutputRefCon;
    
//    if (!decoder->mediaTimeFilter->checkFrame(frame, false)) {
//        
//        av_frame_unref(frame);
//        continue;
//    }
    
    if (decoder->shouldDecode) {
        VTBFrame *frame = new VTBFrame(imageBuffer);
        decoder->frameBuffer.blockInsert(frame);
    }
}

#pragma mark -

TFMPVideoFrameBuffer * VTBFrame::convertToTFMPBuffer(){
    TFMPVideoFrameBuffer *frame = new TFMPVideoFrameBuffer();
    
    frame->width = (int)CVPixelBufferGetWidth(pixelBuffer);
    frame->height = (int)CVPixelBufferGetHeight(pixelBuffer);
    
    OSType pixelType = CVPixelBufferGetPixelFormatType(pixelBuffer);
    if (pixelType == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange ||
        pixelType == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange){
        
        uint8_t *buffer = (uint8_t*)CVPixelBufferGetBaseAddress(pixelBuffer);
        uint8_t *yuv420p = (uint8_t*)malloc(frame->width*frame->height*3/2.0f);
        yuv420sp_to_yuv420p(buffer, yuv420p, frame->width, frame->height);
        
        frame->format = TFMP_VIDEO_PIX_FMT_YUV420P;
        frame->planes = 3;
        uint32_t ysize = frame->width*frame->height;
        frame->pixels[0] = yuv420p;
        frame->pixels[1] = yuv420p+ysize;
        frame->pixels[2] = yuv420p+(5/4)*ysize;
        frame->linesize[0] = frame->width;
        frame->linesize[1] = frame->width/2;
        frame->linesize[2] = frame->width/2;
        
    }else{
        frame->format = TFMP_VIDEO_PIX_FMT_YUV420P;
        frame->planes = 3;
        
        frame->pixels[0] = (uint8_t*)CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 0);
        frame->pixels[1] = (uint8_t*)CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 1);;
        frame->pixels[2] = (uint8_t*)CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 2);
        frame->linesize[0] = frame->width;
        frame->linesize[1] = frame->width/2;
        frame->linesize[2] = frame->width/2;
    }
    
    return frame;
}

#pragma mark -

bool VTBDecoder::initDecoder(){
    if (_decodeSession) {
        return true;
    }
    
    if (_spsSize == 0 || _ppsSize == 0) {
        return false;
    }
    
    const uint8_t * params[2] = {_sps, _pps};
    const size_t paramSize[2] = {_spsSize, _ppsSize};
    OSStatus status = CMVideoFormatDescriptionCreateFromH264ParameterSets(kCFAllocatorDefault, 2, params, paramSize, 4, &_videoFmtDesc);
    if (status != 0) {
        TFMPDLOG_C("create video desc failed!");
        return false;
    }
    //硬解必须是 kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange
    //                                                           或者是kCVPixelFormatType_420YpCbCr8Planar
    //因为iOS是  nv12  其他是nv21
    CFMutableDictionaryRef destImageAttris = CFDictionaryCreateMutable(NULL,0,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);
    CFDictionarySetSInt32(destImageAttris,
                          kCVPixelBufferPixelFormatTypeKey, kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange);
//    CFDictionarySetSInt32(destImageAttris,
//                          kCVPixelBufferWidthKey, width);
//    CFDictionarySetSInt32(destImageAttris,
//                          kCVPixelBufferHeightKey, height);
    CFDictionarySetBoolean(destImageAttris,
                           kCVPixelBufferOpenGLESCompatibilityKey, false);
    
    VTDecompressionOutputCallbackRecord callback = {decodeCallback, this};
    
    status = VTDecompressionSessionCreate(
                                          kCFAllocatorDefault,
                                          _videoFmtDesc,
                                          NULL,
                                          destImageAttris,
                                          &callback,
                                          &_decodeSession);
    
    return status == 0;
}

bool VTBDecoder::prepareDecode(){
    
    shouldDecode = true;
    
    return true;
}

void VTBDecoder::startDecode(){
    pthread_create(&decodeThread, NULL, decodeLoop, this);
    pthread_detach(decodeThread);
}

void VTBDecoder::stopDecode(){
    shouldDecode = false;
}

void *VTBDecoder::decodeLoop(void *context){
    VTBDecoder *decoder = (VTBDecoder *)context;
    
    decoder->isDecoding = true;
    
    AVPacket *pkt = nullptr;
    VTBFrame *frame;
    
    bool frameDelay = false;
    bool delayFramesReleasing = false;
    
    string name = decoder->name;
    while (decoder->shouldDecode) {
        
        if (decoder->pause) {
            myStateObserver.mark(name, 1);
            decoder->isDecoding = false;
            TFMPCondSignal(decoder->waitLoopCond, decoder->waitLoopMutex);
            myStateObserver.mark(name, 11);
            
            pthread_mutex_lock(&decoder->pauseMutex);
            if (decoder->pause) {
                myStateObserver.mark(name, 12);
                pthread_cond_wait(&decoder->pauseCond, &decoder->pauseMutex);
            }else{
                myStateObserver.mark(name, 13);
            }
            pthread_mutex_unlock(&decoder->pauseMutex);
            decoder->isDecoding = true;
        }
        
        pkt = nullptr;
        myStateObserver.mark(name, 2);
        decoder->pktBuffer.blockGetOut(&pkt);
        myStateObserver.mark(name, 3);
        if (pkt == nullptr) {
            
        }
        if (pkt == nullptr) continue;
        
        myStateObserver.mark(name, 4);
        
        
        uint8_t type = pkt->data[3] & 0x1F;
        if (type == 7) {
            int spsSize = pkt->size - 4;
            uint8_t *sps = (uint8_t*)malloc(spsSize);
            memcpy(sps, pkt->data+4, spsSize);
            
            decoder->_spsSize = spsSize;
            decoder->_sps = sps;
            decoder->initDecoder();
        }else if (type == 8){
            int ppsSize = pkt->size - 4;
            uint8_t *pps = (uint8_t*)malloc(ppsSize);
            memcpy(pps, pkt->data+4, ppsSize);
            
            decoder->_ppsSize = ppsSize;
            decoder->_pps = pps;
            decoder->initDecoder();
        }else if (type == 1 || type == 5){
            if (decoder->_decodeSession) {
                decoder->decodePacket(pkt);
            }
        }
        
        if (pkt != nullptr)  av_packet_free(&pkt);
    }
    
    myStateObserver.mark(name, 9);
    decoder->isDecoding = false;
    TFMPCondSignal(decoder->waitLoopCond, decoder->waitLoopMutex);
    myStateObserver.mark(name, 10);
    return 0;
}

void VTBDecoder::decodePacket(AVPacket *pkt){
    int size = pkt->size;
    CMBlockBufferRef buffer = NULL;
    OSStatus status = CMBlockBufferCreateWithMemoryBlock(NULL, pkt->data, size, kCFAllocatorNull, NULL, 0, size, 0, &buffer);
    if (status) {
        TFMPDLOG_C("create block buffer error!");
        return;
    }
    
    uint32_t len = CFSwapInt32BigToHost((uint32_t)size-4);
    status = CMBlockBufferReplaceDataBytes(&len, buffer, 0, 4);
    if (status != 0) {
        TFMPDLOG_C("replace buffer header error!");
    }
    
    CMSampleBufferRef sample = NULL;
    //    const size_t sampleSize[] = {size};
    status = CMSampleBufferCreate(kCFAllocatorDefault,
                                  buffer,
                                  true, 0, 0,
                                  _videoFmtDesc,
                                  1,0,NULL, 0, NULL, &sample);
    if (status || sample == nil) {
        TFMPDLOG_C("create sample buffer error!");
        return;
    }
    
    CVPixelBufferRef pixelBuffer;
    VTDecodeInfoFlags outFlags;
    status = VTDecompressionSessionDecodeFrame(_decodeSession, sample, 0, &pixelBuffer, &outFlags);
    if (status) {
        TFMPDLOG_C("decode frame error: %d",status);
    }
}

void VTBDecoder::insertPacket(AVPacket *packet){
    
    AVPacket *refPkt = av_packet_alloc();
    av_packet_ref(refPkt, packet);
    
    pktBuffer.blockInsert(refPkt);
}

void VTBDecoder::activeBlock(bool flag){
    pktBuffer.disableIO(!flag);
    frameBuffer.disableIO(!flag);
}

void VTBDecoder::flush(){
    
    string stateName = name+" flush";
    
    //1. prevent from starting next decode loop
    pause = true;
    
    //2. disable buffer's in and out to invalid the frame or packet in current loop.
    myStateObserver.mark(stateName, 1);
    pktBuffer.disableIO(true);
    frameBuffer.disableIO(true);
    myStateObserver.mark(stateName, 1);
    
    //3. wait for the end of current loop
    pthread_mutex_lock(&waitLoopMutex);
    if (isDecoding) {
        myStateObserver.mark(stateName, 3);
        pthread_cond_wait(&waitLoopCond, &waitLoopMutex);
    }else{
        myStateObserver.mark(stateName, 4);
    }
    pthread_mutex_unlock(&waitLoopMutex);
    myStateObserver.mark(stateName, 5);
    
    //4. flush all reserved buffers
    pktBuffer.flush();
    myStateObserver.mark(stateName, 6);
    frameBuffer.flush();
    myStateObserver.mark(stateName, 7);
    
    //5. flush FFMpeg's buffer.
    //If dont'f call this, there are some new packets which contains old frames.
    flushContext();
    myStateObserver.mark(stateName, 8);
    
    //6. resume the decode loop
    pktBuffer.disableIO(false);
    frameBuffer.disableIO(false);
    
    pause = false;
    myStateObserver.mark(stateName, 9);
    TFMPCondSignal(pauseCond, pauseMutex);
    myStateObserver.mark(stateName, 10);
}

void VTBDecoder::flushContext(){
    
}

bool VTBDecoder::bufferIsEmpty(){
    return pktBuffer.isEmpty() && frameBuffer.isEmpty();
}

void VTBDecoder::freeResources(){
    shouldDecode = false;
    
    //2. disable buffer's in and out to invalid the frame or packet in current loop.
    pktBuffer.disableIO(true);
    frameBuffer.disableIO(true);
    //3. wait for the end of current loop
    myStateObserver.mark(name+" free", 1);
    pthread_mutex_lock(&waitLoopMutex);
    if (isDecoding) {
        pthread_cond_wait(&waitLoopCond, &waitLoopMutex);
        myStateObserver.mark(name+" free", 2);
    }else{
        myStateObserver.mark(name+" free", 3);
    }
    pthread_mutex_unlock(&waitLoopMutex);
    myStateObserver.mark(name+" free", 4);
    //4. flush all reserved buffers
    pktBuffer.flush();
    frameBuffer.flush();
    myStateObserver.mark(name+" free", 5);
    flushContext();
}


#pragma mark -


