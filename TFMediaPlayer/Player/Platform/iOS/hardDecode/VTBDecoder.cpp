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

inline void freePacket(AVPacket **pkt){
    av_packet_free(pkt);
}

inline void freeFrame(VTBFrame **frame){
    VTBFrame::free(frame);
}

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

static void CFDictionarySetString(CFMutableDictionaryRef dictionary, CFStringRef key, const char *chars)
{
    CFStringRef string;
    string = CFStringCreateWithCString(kCFAllocatorDefault, chars, kCFStringEncodingUTF8);
    CFDictionarySetValue(dictionary, key, string);
    CFRelease(string);
}

static void CFDictionarySetData(CFMutableDictionaryRef dict, CFStringRef key, uint8_t * value, uint64_t length)
{
    CFDataRef data;
    data = CFDataCreate(NULL, value, (CFIndex)length);
    CFDictionarySetValue(dict, key, data);
    CFRelease(data);
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
        AVPacket *pkt = (AVPacket*)sourceFrameRefCon;
        frame->pts = pkt->pts;
        decoder->frameBuffer.blockInsert(frame);
    }
}

#pragma mark -

TFMPVideoFrameBuffer * VTBFrame::convertToTFMPBuffer(){
    TFMPVideoFrameBuffer *frame = new TFMPVideoFrameBuffer();
    
    CVPixelBufferLockBaseAddress(pixelBuffer, 0);
    frame->width = (int)CVPixelBufferGetWidth(pixelBuffer);
    frame->height = (int)CVPixelBufferGetHeight(pixelBuffer);
    
    OSType pixelType = CVPixelBufferGetPixelFormatType(pixelBuffer);
    if (pixelType == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange ||
        pixelType == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange){
        
        uint8_t *yPlane = (uint8_t*)CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 0);
        uint8_t *uvPlane = (uint8_t*)CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 1);
        uint8_t *yuv420p = (uint8_t*)malloc(frame->width*frame->height*3/2.0f);
        nv12_to_yuv420p(yPlane, uvPlane, yuv420p, frame->width, frame->height);
        
        frame->format = TFMP_VIDEO_PIX_FMT_YUV420P;
        frame->planes = 3;
        uint32_t ysize = frame->width*frame->height;
        frame->pixels[0] = yuv420p;
        frame->pixels[1] = yuv420p+ysize;
        frame->pixels[2] = yuv420p+ysize/4*5;
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
    
    CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
    
    return frame;
}

#pragma mark -

static CMFormatDescriptionRef CreateFormatDescriptionFromCodecData(CMVideoCodecType format_id, int width, int height, const uint8_t *extradata, int extradata_size, uint32_t atom)
{
    CMFormatDescriptionRef fmt_desc = NULL;
    OSStatus status;
    
    CFMutableDictionaryRef par = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);
    CFMutableDictionaryRef atoms = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);
    CFMutableDictionaryRef extensions = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    
    /* CVPixelAspectRatio dict */
    CFDictionarySetSInt32(par, CFSTR ("HorizontalSpacing"), 0);
    CFDictionarySetSInt32(par, CFSTR ("VerticalSpacing"), 0);
    
    /* SampleDescriptionExtensionAtoms dict */
    switch (format_id) {
        case kCMVideoCodecType_H264:
            CFDictionarySetData(atoms, CFSTR ("avcC"), (uint8_t *)extradata, extradata_size);
            break;
        case kCMVideoCodecType_HEVC:
            CFDictionarySetData(atoms, CFSTR ("hvcC"), (uint8_t *)extradata, extradata_size);
            break;
        default:
            break;
    }
    
    
    /* Extensions dict */
    CFDictionarySetString(extensions, CFSTR ("CVImageBufferChromaLocationBottomField"), "left");
    CFDictionarySetString(extensions, CFSTR ("CVImageBufferChromaLocationTopField"), "left");
    CFDictionarySetBoolean(extensions, CFSTR("FullRangeVideo"), FALSE);
    CFDictionarySetValue(extensions, CFSTR ("CVPixelAspectRatio"), (CFTypeRef *) par);
    CFDictionarySetValue(extensions, CFSTR ("SampleDescriptionExtensionAtoms"), (CFTypeRef *) atoms);
    status = CMVideoFormatDescriptionCreate(NULL, format_id, width, height, extensions, &fmt_desc);
    
    CFRelease(extensions);
    CFRelease(atoms);
    CFRelease(par);
    
    if (status == 0)
        return fmt_desc;
    else
        return NULL;
}

bool VTBDecoder::prepareDecode(){
    
    AVCodec *codec = avcodec_find_decoder(fmtCtx->streams[steamIndex]->codecpar->codec_id);
    if (codec == nullptr) {
        printf("find codec type: %d error\n",type);
        return false;
    }
    
    codecCtx = avcodec_alloc_context3(codec);
    if (codecCtx == nullptr) {
        printf("alloc codecContext type: %d error\n",type);
        return false;
    }
    
    avcodec_parameters_to_context(codecCtx, fmtCtx->streams[steamIndex]->codecpar);
    
    AVCodecParameters *codecpar = fmtCtx->streams[steamIndex]->codecpar;
    uint8_t *extradata = codecpar->extradata;
    
    if (extradata[0] == 1) {
        TFMPDLOG_C("nalu start with nalu length");
        _videoFmtDesc = CreateFormatDescriptionFromCodecData(kCMVideoCodecType_H264, codecpar->width, codecpar->height, codecpar->extradata, codecpar->extradata_size, 0);
    }else{
        TFMPDLOG_C("nalu start with start code");
        return false;
    }
    
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
    
    VTDecompressionSessionCreate(
                                  kCFAllocatorDefault,
                                  _videoFmtDesc,
                                  NULL,
                                  destImageAttris,
                                  &callback,
                                  &_decodeSession);
    
    shouldDecode = true;
    
#if DEBUG
    if (type == AVMEDIA_TYPE_AUDIO) {
        strcpy(frameBuffer.name, "audio_frame");
        strcpy(pktBuffer.name, "audio_packet");
    }else if (type == AVMEDIA_TYPE_VIDEO){
        strcpy(frameBuffer.name, "video_frame");
        strcpy(pktBuffer.name, "video_packet");
    }else{
        strcpy(frameBuffer.name, "subtitle_frame");
        strcpy(pktBuffer.name, "subtitle_packet");
    }
#endif
    
    pktBuffer.valueFreeFunc = freePacket;
    frameBuffer.valueFreeFunc = freeFrame;
    
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
        if (decoder->_decodeSession) {
            decoder->decodePacket(pkt);
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
    
    VTDecodeInfoFlags outFlags;
    status = VTDecompressionSessionDecodeFrame(_decodeSession, sample, 0, pkt, &outFlags);
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


