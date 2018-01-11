//
//  AudioResampler.cpp
//  TFMediaPlayer
//
//  Created by shiwei on 18/1/11.
//  Copyright © 2018年 shiwei. All rights reserved.
//

#include "AudioResampler.hpp"
#include "TFMPDebugFuncs.h"

#pragma mark - resample audio

using namespace tfmpcore;

/** If source audio desc is different from adopted audio desc, we need to resample source audio */
inline bool _isNeedResample(AVFrame *sourceFrame, TFMPAudioStreamDescription *destDesc){
    
    //bad frame data
    if (sourceFrame->sample_rate <= 0 ||
        sourceFrame->channel_layout <= 0 ||
        sourceFrame->extended_data == nullptr) {
        printf("bad frame data ^^^^^\n");
        return false;
    }
    
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

bool AudioResampler::isNeedResample(AVFrame *sourceFrame){
    return _isNeedResample(sourceFrame,&adoptedAudioDesc);
}

void AudioResampler::initResampleContext(AVFrame *sourceFrame){
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
    
    TFCheckRetval("swr init");
    
    if (lastSourceAudioDesc != nullptr) free(lastSourceAudioDesc);
    
    lastSourceAudioDesc = new TFMPAudioStreamDescription();
    lastSourceAudioDesc->sampleRate = sourceFrame->sample_rate;
    lastSourceAudioDesc->formatFlags = formatFlagsFromFFmpegAudioFormat(sourceFmt);
    lastSourceAudioDesc->bitsPerChannel = bitPerChannelFromFFmpegAudioFormat(sourceFmt);
    lastSourceAudioDesc->ffmpeg_channel_layout = sourceFrame->channel_layout;
    lastSourceAudioDesc->channelsPerFrame = sourceFrame->channels;
}

uint8_t **AudioResampler::reampleAudioFrame(AVFrame *inFrame, int *outSamples, int *linesize){
    
    if (_isNeedResample(inFrame, lastSourceAudioDesc)) {
        initResampleContext(inFrame);
    }
    
    if (swrCtx == nullptr) {
        return nullptr;
    }
    
    int nb_samples = (int)av_rescale_rnd(swr_get_delay(swrCtx, adoptedAudioDesc.sampleRate) + inFrame->nb_samples,adoptedAudioDesc.sampleRate, inFrame->sample_rate, AV_ROUND_UP);
    
    AVSampleFormat destFmt = FFmpegAudioFormatFromTFMPAudioDesc(adoptedAudioDesc.formatFlags, adoptedAudioDesc.bitsPerChannel);
    
    printf("outCount: %d, sourceCount: %d",*outSamples, inFrame->nb_samples);
    
    uint8_t **outBuffer = (uint8_t**)malloc(sizeof(uint8_t*));
    int retval = av_samples_alloc(outBuffer,
                                  linesize,
                                  adoptedAudioDesc.channelsPerFrame,
                                  nb_samples,
                                  destFmt, 0);
    
    if (retval < 0) {
        printf("av_samples_alloc error\n");
        return nullptr;
    }
    
    //    uint8_t* m_ain[SWR_CH_MAX];
    //    setup_array(m_ain, inFrame, (AVSampleFormat)inFrame->format, inFrame->nb_samples);
    
    swr_convert(swrCtx, outBuffer, nb_samples, (const uint8_t **)inFrame->extended_data, inFrame->nb_samples);
    
    *outSamples = nb_samples;
    return outBuffer;
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
