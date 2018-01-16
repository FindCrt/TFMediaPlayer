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
    
    if (resampleSize == 0) {
        TFMPDLOG_C("memory alloc resample buffer error!\n");
        return false;
    }
    
    uint8_t **outBuffer = &resampledBuffers;
    int actualOutSamples = swr_convert(swrCtx, outBuffer, nb_samples, (const uint8_t **)inFrame->extended_data, inFrame->nb_samples);
    
    if (actualOutSamples == 0) {
        TFMPDLOG_C("memory alloc resample buffer error!\n");
        return false;
    }
    
    unsigned int actualOutSize = actualOutSamples * adoptedAudioDesc.channelsPerFrame * av_get_bytes_per_sample(destFmt);
    
//    printf("samples:%d --> %d, size:%d --> %d \n",nb_samples, actualOutSamples, outsize, actualOutSize);
    
    *outSamples = actualOutSamples;
    *linesize = actualOutSize;
    
    resampleSize = outsize;
    
    return true;
}
