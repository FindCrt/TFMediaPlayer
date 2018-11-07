//
//  VTBDecoder.cpp
//  TFMediaPlayer
//
//  Created by shiwei on 2018/9/28.
//  Copyright © 2018年 shiwei. All rights reserved.
//

#include "VTBDecoder.h"
#include "TFMPUtilities.h"
#include "TFMPDebugFuncs.h"

using namespace tfmpcore;

#pragma mark -

TFMPVideoFrameBuffer * VTBDecoder::displayBufferFromPixelBuffer(CVPixelBufferRef pixelBuffer){
    
    TFMPVideoFrameBuffer *frame = new TFMPVideoFrameBuffer();
    frame->opaque = CVPixelBufferRetain(pixelBuffer);
    
    frame->width = (int)CVPixelBufferGetWidth(pixelBuffer);
    frame->height = (int)CVPixelBufferGetHeight(pixelBuffer);
    
    CVPixelBufferLockBaseAddress(pixelBuffer, 0);
    
    frame->format = TFMP_VIDEO_PIX_FMT_NV12_VTB;
    frame->planes = 2;
    frame->pixels[0] = (uint8_t*)CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 0);
    frame->pixels[1] = (uint8_t*)CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 1);
    
    frame->linesize[0] = (int)CVPixelBufferGetWidthOfPlane(pixelBuffer, 0);
    frame->linesize[1] = (int)CVPixelBufferGetWidthOfPlane(pixelBuffer, 1);
    
    CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
    
    return frame;
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
    
    TFMPPacket *packet = (TFMPPacket*)sourceFrameRefCon;
    if (status != 0) {
        av_packet_free(&(packet->pkt));
        delete packet;
        return;
    }
    
    VTBDecoder *decoder = (VTBDecoder *)decompressionOutputRefCon;
    
    if (decoder->shouldDecode) {
        TFMPFrame *tfmpFrame = new TFMPFrame();
        tfmpFrame->serial = packet->serial;  //直接从packet继承serial,那么一定不会错
        tfmpFrame->type = TFMPFrameTypeVTBVideo;
        tfmpFrame->pts = packet->pkt->pts;
        tfmpFrame->freeFrameFunc = VTBDecoder::freeFrame;
        tfmpFrame->displayBuffer = VTBDecoder::displayBufferFromPixelBuffer(imageBuffer);

        myStateObserver.mark("video gen", 1, true);
        decoder->frameBuffer.blockInsert(tfmpFrame);
    }
    
    av_packet_free(&(packet->pkt));
    delete packet;
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
    
    NSData *data = [[NSData alloc] initWithBytes:extradata length:extradata_size];
    NSLog(@"[%d, %d]avcC: %@",width,height,data);
    
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

#pragma mark -

bool VTBDecoder::prepareDecode(){
    
    AVCodec *codec = avcodec_find_decoder(fmtCtx->streams[streamIndex]->codecpar->codec_id);
    if (codec == nullptr) {
        printf("find codec type: %d error\n",type);
        return false;
    }
    
    codecCtx = avcodec_alloc_context3(codec);
    if (codecCtx == nullptr) {
        printf("alloc codecContext type: %d error\n",type);
        return false;
    }
    
    avcodec_parameters_to_context(codecCtx, fmtCtx->streams[streamIndex]->codecpar);
    
    AVCodecParameters *codecpar = fmtCtx->streams[streamIndex]->codecpar;
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
    
    OSStatus status = VTDecompressionSessionCreate(
                                  kCFAllocatorDefault,
                                  _videoFmtDesc,
                                  NULL,
                                  destImageAttris,
                                  &callback,
                                  &_decodeSession);
    
    shouldDecode = status == 0 && _decodeSession;
    
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
    frameBuffer.valueCompFunc = frameCompare;
    
    return true;
}

void *VTBDecoder::decodeLoop(void *context){
    
    VTBDecoder *decoder = (VTBDecoder *)context;
    
    TFMPPacket *packet = nullptr;
    while (decoder->shouldDecode) {
        do {
            if (packet) {
                av_packet_free(&(packet->pkt));
                delete packet;
            }
            packet = new TFMPPacket(0, nullptr);
            decoder->pktBuffer.blockGetOut(packet);
            if (packet->pkt == nullptr) {
                delete packet;
                packet = nullptr;
                break;
            };
        } while (packet->serial != decoder->serial);
        
        if (packet) {
            decoder->decodePacket(packet);
            packet = nullptr;
        }
    }
    return 0;
}

void VTBDecoder::decodePacket(TFMPPacket *packet){
    
    AVPacket *pkt = packet->pkt;
    if (pkt == nullptr) {
        return;
    }
    
    int size = pkt->size;
    CMBlockBufferRef buffer = NULL;
    OSStatus status = CMBlockBufferCreateWithMemoryBlock(NULL, pkt->data, size, kCFAllocatorNull, NULL, 0, size, 0, &buffer);
    
    if (status) {
        TFMPDLOG_C("create block buffer error!");
        av_packet_free(&(packet->pkt));
        delete packet;
        CFRelease(buffer);
        return;
    }
    
    CMSampleBufferRef sample = NULL;
    //    const size_t sampleSize[] = {size};
    status = CMSampleBufferCreate(kCFAllocatorDefault,
                                  buffer,
                                  true, 0, 0,
                                  _videoFmtDesc,
                                  1,0,NULL, 0, NULL, &sample);
    CFRelease(buffer);
    if (status || sample == nil) {
        av_packet_free(&(packet->pkt));
        delete packet;
        return;
    }
    VTDecodeInfoFlags outFlags;
    status = VTDecompressionSessionDecodeFrame(_decodeSession, sample, 0, packet, &outFlags);
    if (status) {
        TFMPDLOG_C("decode frame error: %d",status);
    }
    CFRelease(sample);
}

void VTBDecoder::stopDecode(){
    Decoder::stopDecode();
    
    //释放vtb相关资源
    VTDecompressionSessionInvalidate(_decodeSession);
    CFRelease(_decodeSession);
    
    CFRelease(_videoFmtDesc);
}


