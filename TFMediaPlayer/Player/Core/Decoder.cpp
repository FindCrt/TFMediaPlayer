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

inline bool isNeedResample(AVFrame *sourceFrame, TFMPAudioStreamDescription *destDesc);
inline bool isNeedChangeSwrContext(AVFrame *sourceFrame, TFMPAudioStreamDescription *lastDesc);
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
    AVFrame *frame;
    
    while (decoder->shouldDecode) {
        
        decoder->pktBuffer.blockGetOut(&pkt);
        int retval = avcodec_send_packet(decoder->codecCtx, &pkt);
        if (retval != 0) {
            TFCheckRetval("avcodec send packet");
            continue;
        }
        
        if (decoder->type == AVMEDIA_TYPE_AUDIO) { //may many frames in one packet.
            
            while (retval == 0) {
                frame = av_frame_alloc();
                retval = avcodec_receive_frame(decoder->codecCtx, frame);
                
                if (isNeedResample(frame, &(decoder->adoptedAudioDesc))) {
                    
                    //source audio desc may change.
                    if (isNeedResample(frame, (decoder->lastSourceAudioDesc))) {
                        decoder->initResampleContext(frame);
                    }
                    
                    AVFrame *resampledFrame = av_frame_alloc();
                    if (!decoder->reampleAudioFrame(frame, resampledFrame)) {
                        continue;
                    }
                    
                    frame = resampledFrame;
                }
                
                decoder->frameBuffer.blockInsert(frame);
                
                
                if (retval != 0 && retval != AVERROR_EOF) {
                    TFCheckRetval("avcodec receive frame");
                    continue;
                }
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

#pragma mark - resample audio

/** If source audio desc is different from adopted audio desc, we need to resample source audio */
inline bool isNeedResample(AVFrame *sourceFrame, TFMPAudioStreamDescription *destDesc){
    
    if (destDesc == nullptr) return true;
    
    if (destDesc->sampleRate != sourceFrame->sample_rate) return true;
    if (destDesc->channelsPerFrame != sourceFrame->channels) return true;
    
    if (formatFlagsFromFFmpegAudioFormat((AVSampleFormat)sourceFrame->format) != destDesc->formatFlags) {
        return true;
    }
    if (bitPerChannelFromFFmpegAudioFormat((AVSampleFormat)sourceFrame->format) != destDesc->bitsPerChannel){
        return true;
    }
    
    return false;
}

void Decoder::initResampleContext(AVFrame *sourceFrame){
    swrCtx = swr_alloc();
    
    AVSampleFormat destFmt = FFmpegAudioFormatFromTFMPAudioDesc(adoptedAudioDesc.formatFlags, adoptedAudioDesc.bitsPerChannel);
    AVSampleFormat sourceFmt = (AVSampleFormat)sourceFrame->format;
    
    swrCtx = swr_alloc_set_opts(swrCtx,
                                adoptedAudioDesc.ffmpeg_channel_layout,
                                destFmt,
                                adoptedAudioDesc.sampleRate,
                                sourceFrame->channel_layout,
                                sourceFmt,
                                sourceFrame->sample_rate,
                                0, NULL);
    int retval = swr_init(swrCtx);
    
    
    if (lastSourceAudioDesc != nullptr) free(lastSourceAudioDesc);
    
    lastSourceAudioDesc = new TFMPAudioStreamDescription();
    lastSourceAudioDesc->sampleRate = sourceFrame->sample_rate;
    lastSourceAudioDesc->formatFlags = formatFlagsFromFFmpegAudioFormat(sourceFmt);
    lastSourceAudioDesc->bitsPerChannel = bitPerChannelFromFFmpegAudioFormat(sourceFmt);
    lastSourceAudioDesc->ffmpeg_channel_layout = sourceFrame->channel_layout;
    lastSourceAudioDesc->channelsPerFrame = sourceFrame->channels;
}

bool Decoder::reampleAudioFrame(AVFrame *inFrame, AVFrame *outFrame){
    
    if (swrCtx == nullptr) {
        return false;
    }
    
    outFrame->nb_samples = (int)av_rescale_rnd(swr_get_delay(swrCtx, adoptedAudioDesc.sampleRate) + inFrame->nb_samples,adoptedAudioDesc.sampleRate, inFrame->sample_rate, AV_ROUND_UP);
    
    AVSampleFormat destFmt = FFmpegAudioFormatFromTFMPAudioDesc(adoptedAudioDesc.formatFlags, adoptedAudioDesc.bitsPerChannel);
    
    printf("outCount: %d, sourceCount: %d",outFrame->nb_samples, inFrame->nb_samples);
    int retval = av_samples_alloc(outFrame->extended_data,
                               &outFrame->linesize[0],
                               adoptedAudioDesc.channelsPerFrame,
                               outFrame->nb_samples,
                               destFmt, 0);
    
    if (retval < 0) {
        printf("av_samples_alloc error\n");
        return false;
    }
    
//    uint8_t* m_ain[SWR_CH_MAX];
//    setup_array(m_ain, inFrame, (AVSampleFormat)inFrame->format, inFrame->nb_samples);
    
    swr_convert(swrCtx, outFrame->extended_data, outFrame->nb_samples, (const uint8_t **)inFrame->extended_data, inFrame->nb_samples);
    return true;
}

//static void setup_array(uint8_t* out[SWR_CH_MAX], AVFrame* in_frame, int format, int samples)
//{
//    if (av_sample_fmt_is_planar((AVSampleFormat)format))
//    {
//        int i;int plane_size = av_get_bytes_per_sample((AVSampleFormat)(format & 0xFF)) * samples;format &= 0xFF;
//        
//        for (i = 0; i < in_frame->channels; i++)
//        {
//            out[i] = in_frame->data[i];
//        }
//    }
//    else
//    {
//        out[0] = in_frame->data[0];
//    }
//}

