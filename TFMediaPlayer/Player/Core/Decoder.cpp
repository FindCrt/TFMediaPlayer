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

using namespace tfmpcore;

static int SWR_CH_MAX = 2;

//inline bool isNeedResample(AVFrame *sourceFrame, TFMPAudioStreamDescription *destDesc);
//inline bool isNeedChangeSwrContext(AVFrame *sourceFrame, TFMPAudioStreamDescription *lastDesc);
//static void setup_array(uint8_t* out[SWR_CH_MAX], AVFrame* in_frame, int format, int samples);

bool Decoder::prepareDecode(){
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
    
    int retval = avcodec_open2(codecCtx, codec, NULL);
    if (retval < 0) {
        printf("avcodec_open2 id: %d error\n",codec->id);
        return false;
    }
    
#if DEBUG
    if (type == AVMEDIA_TYPE_AUDIO) {
        strcpy(frameBuffer.identifier, "audio_frame");
        strcpy(pktBuffer.identifier, "audio_packet");
    }else if (type == AVMEDIA_TYPE_VIDEO){
        strcpy(frameBuffer.identifier, "video_frame");
        strcpy(pktBuffer.identifier, "video_packet");
    }else{
        strcpy(frameBuffer.identifier, "subtitle_frame");
        strcpy(pktBuffer.identifier, "subtitle_packet");
    }
#endif
    
    shouldDecode = true;
    
    return true;
}

void Decoder::startDecode(){
    pthread_create(&decodeThread, NULL, decodeLoop, this);
}

void Decoder::stopDecode(){
    shouldDecode = false;
}

void Decoder::decodePacket(AVPacket *packet){
    
    AVPacket *refPacket = av_packet_alloc();
    av_packet_ref(refPacket, packet);
    
    pktBuffer.blockInsert(*refPacket);
}

void *Decoder::decodeLoop(void *context){
    
    Decoder *decoder = (Decoder *)context;
    
    AVPacket pkt;
    AVFrame *frame = av_frame_alloc();
    
    while (decoder->shouldDecode) {
        
        decoder->pktBuffer.blockGetOut(&pkt);
        int retval = avcodec_send_packet(decoder->codecCtx, &pkt);
        if (retval != 0) {
            TFCheckRetval("avcodec send packet");
            continue;
        }
        
        if (decoder->type == AVMEDIA_TYPE_AUDIO) { //may many frames in one packet.
            
            while (retval == 0) {
//                frame = av_frame_alloc();
                retval = avcodec_receive_frame(decoder->codecCtx, frame);
                
                if (retval != 0 && retval != AVERROR_EOF) {
                    TFCheckRetval("avcodec receive frame");
                    continue;
                }
                
//                if (isNeedResample(frame, &(decoder->adoptedAudioDesc))) {
//                    
//                    //source audio desc may change.
//                    if (isNeedResample(frame, (decoder->lastSourceAudioDesc))) {
//                        decoder->initResampleContext(frame);
//                    }
//                    
//                    AVFrame *resampledFrame = av_frame_alloc();
//                    if (!decoder->reampleAudioFrame(frame, resampledFrame)) {
//                        continue;
//                    }
//                    
//                    decoder->frameBuffer.blockInsert(resampledFrame);
//                }else{
//                    decoder->frameBuffer.blockInsert(frame);
//                }
                
                decoder->frameBuffer.blockInsert(frame);
            }
        }else{
            
            frame = av_frame_alloc();
            retval = avcodec_receive_frame(decoder->codecCtx, frame);
            decoder->frameBuffer.blockInsert(frame);
            
            if (retval != 0) {
                TFCheckRetval("avcodec receive frame");
                continue;
            }
        }
        
        av_packet_unref(&pkt);
    }
    
    return 0;
}


