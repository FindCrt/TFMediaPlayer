//
//  AudioResampler.cpp
//  TFMediaPlayer
//
//  Created by shiwei on 18/1/11.
//  Copyright © 2018年 shiwei. All rights reserved.
//

#include "AudioResampler.hpp"
#include "TFMPDebugFuncs.h"
extern "C"{
#include <libavcodec/avcodec.h>
}

#pragma mark - resample audio

using namespace tfmpcore;

/** If source audio desc is different from adopted audio desc, we need to resample source audio */
inline bool _isNeedResample(AVFrame *sourceFrame, TFMPAudioStreamDescription *destDesc){
    
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

void AudioResampler::freeResources(){
    
    if (swrCtx) swr_free(&swrCtx);
    
    free(resampledBuffers);
    resampledBuffers = resampledBuffers1 = nullptr;
    
    free(lastSourceAudioDesc);
    lastSourceAudioDesc = nullptr;
    
    resampleSize = 0;
    adoptedAudioDesc = {};
    
}

void AudioResampler::initResampleContext(AVFrame *sourceFrame){
    
    if (sourceFrame->sample_rate == 0 ||
        sourceFrame->channel_layout == 0 ||
        sourceFrame->format == 0) {
        
        return;
    }
    
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

bool AudioResampler::reampleAudioFrame(AVFrame *inFrame, int *outSamples, int *linesize){
    
    if (_isNeedResample(inFrame, lastSourceAudioDesc)) {
        initResampleContext(inFrame);
    }
    
    if (swrCtx == nullptr) {
        return nullptr;
    }
    
//    int nb_samples = (int)av_rescale_rnd(swr_get_delay(swrCtx, adoptedAudioDesc.sampleRate) + inFrame->nb_samples,adoptedAudioDesc.sampleRate, inFrame->sample_rate, AV_ROUND_UP);
    int nb_samples = swr_get_out_samples(swrCtx, inFrame->nb_samples);
    
    AVSampleFormat destFmt = FFmpegAudioFormatFromTFMPAudioDesc(adoptedAudioDesc.formatFlags, adoptedAudioDesc.bitsPerChannel);
    int outsize = av_samples_get_buffer_size(linesize, adoptedAudioDesc.channelsPerFrame, nb_samples, destFmt, 0);
    
    //av_fast_malloc(&resampledBuffers, &resampleSize, outsize);
    if (resampleSize < outsize) {
        free(resampledBuffers);
        resampledBuffers = (uint8_t *)malloc(outsize);
        resampleSize = outsize;
    }
    
    uint8_t **outBuffer = &resampledBuffers;
    int actualOutSamples = swr_convert(swrCtx, outBuffer, nb_samples, (const uint8_t **)inFrame->extended_data, inFrame->nb_samples);
    
    if (actualOutSamples == 0) {
        
        return false;
    }
    
    unsigned int actualOutSize = actualOutSamples * adoptedAudioDesc.channelsPerFrame * av_get_bytes_per_sample(destFmt);
    
    *outSamples = actualOutSamples;
    *linesize = actualOutSize;
    
    resampleSize = actualOutSize;
    
    return true;
}

bool AudioResampler::reampleAudioFrame2(AVFrame *inFrame, int *outSamples, int *linesize){
    
    AVSampleFormat destFmt = FFmpegAudioFormatFromTFMPAudioDesc(adoptedAudioDesc.formatFlags, adoptedAudioDesc.bitsPerChannel);
    AVSampleFormat sourceFmt = (AVSampleFormat)inFrame->format;
    
    if (_isNeedResample(inFrame, lastSourceAudioDesc)) {
        
        uint64_t dec_channel_layout =
        (inFrame->channel_layout && inFrame->channels == av_get_channel_layout_nb_channels(inFrame->channel_layout)) ?
        inFrame->channel_layout : av_get_default_channel_layout(inFrame->channels);
        
        swrCtx = swr_alloc_set_opts(NULL,
                                    adoptedAudioDesc.ffmpeg_channel_layout,
                                    destFmt,
                                    adoptedAudioDesc.sampleRate,
                                    dec_channel_layout,
                                    sourceFmt,
                                    inFrame->sample_rate,
                                    0, NULL);
        if (swrCtx == nullptr || swr_init(swrCtx) < 0) {
            swr_free(&swrCtx);
            return false;
        }
        
        lastSourceAudioDesc = new TFMPAudioStreamDescription();
        lastSourceAudioDesc->sampleRate = inFrame->sample_rate;
        lastSourceAudioDesc->formatFlags = formatFlagsFromFFmpegAudioFormat(sourceFmt);
        lastSourceAudioDesc->bitsPerChannel = bitPerChannelFromFFmpegAudioFormat(sourceFmt);
        lastSourceAudioDesc->ffmpeg_channel_layout = inFrame->channel_layout;
        lastSourceAudioDesc->channelsPerFrame = inFrame->channels;
    }
    
    if (!swrCtx) {
        return false;
    }
    
    const uint8_t **in = (const uint8_t **)inFrame->extended_data;
    uint8_t **out = &resampledBuffers1;
    int out_count = (int64_t)inFrame->nb_samples * adoptedAudioDesc.sampleRate / inFrame->sample_rate + 256;
    int out_size  = av_samples_get_buffer_size(NULL, adoptedAudioDesc.channelsPerFrame, out_count, destFmt, 0);
    
    int len2;
    if (out_size < 0) {
        av_log(NULL, AV_LOG_ERROR, "av_samples_get_buffer_size() failed\n");
        return -1;
    }
//    if (wanted_nb_samples != af->frame->nb_samples) {
//        if (swr_set_compensation(is->swr_ctx, (wanted_nb_samples - af->frame->nb_samples) * is->audio_tgt.freq / af->frame->sample_rate,
//                                 wanted_nb_samples * is->audio_tgt.freq / af->frame->sample_rate) < 0) {
//            av_log(NULL, AV_LOG_ERROR, "swr_set_compensation() failed\n");
//            return -1;
//        }
//    }
    av_fast_malloc(&resampledBuffers1, &resampleSize, out_size);
    if (resampledBuffers1 == nullptr)
        return AVERROR(ENOMEM);
    len2 = swr_convert(swrCtx, out, out_count, in, inFrame->nb_samples);
    if (len2 < 0) {
        av_log(NULL, AV_LOG_ERROR, "swr_convert() failed\n");
        return -1;
    }
    
    if (len2 == out_count) {
        av_log(NULL, AV_LOG_WARNING, "audio buffer is probably too small\n");
        if (swr_init(swrCtx) < 0)
            swr_free(&swrCtx);
    }
    
    resampledBuffers = resampledBuffers1;
    resampleSize = len2 * adoptedAudioDesc.channelsPerFrame * av_get_bytes_per_sample(destFmt);
    
    *outSamples = len2;
    *linesize = resampleSize;
    
    return true;
}

