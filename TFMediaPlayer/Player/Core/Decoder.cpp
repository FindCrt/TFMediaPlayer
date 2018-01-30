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
//#include "buffer_internal.h"

using namespace tfmpcore;

struct tf_AVBuffer {
    uint8_t *data; /**< data described by this buffer */
    int      size; /**< size of data in bytes */
    
    /**
     *  number of existing AVBufferRef instances referring to this buffer
     */
    std::atomic_uint refcount;
    
    /**
     * a callback for freeing the data
     */
    void (*free)(void *opaque, uint8_t *data);
    
    /**
     * an opaque pointer, to be used by the freeing callback
     */
    void *opaque;
    
    /**
     * A combination of BUFFER_FLAG_*
     */
    int flags;
};
struct tf_AVBufferPool;
typedef struct tf_BufferPoolEntry {
    uint8_t *data;
    
    /*
     * Backups of the original opaque/free of the AVBuffer corresponding to
     * data. They will be used to free the buffer when the pool is freed.
     */
    void *opaque;
    void (*free)(void *opaque, uint8_t *data);
    
    tf_AVBufferPool *pool;
    struct BufferPoolEntry *next;
} tf_BufferPoolEntry;


#define AVMutex char
struct tf_AVBufferPool {
    AVMutex mutex;
    BufferPoolEntry *pool;
    
    /*
     * This is used to track when the pool is to be freed.
     * The pointer to the pool itself held by the caller is considered to
     * be one reference. Each buffer requested by the caller increases refcount
     * by one, returning the buffer to the pool decreases it by one.
     * refcount reaches zero when the buffer has been uninited AND all the
     * buffers have been released, then it's safe to free the pool and all
     * the buffers in it.
     */
    std::atomic_uint refcount;
    
    int size;
    void *opaque;
    AVBufferRef* (*alloc)(int size);
    AVBufferRef* (*alloc2)(void *opaque, int size);
    void         (*pool_free)(void *opaque);
};

inline void logBufs(AVFrame *frame, char *tag){
    TFMPDLOG_C("\n---------%s-----------\n",tag);
    TFMPDLOG_C("frame: %x buf:%x",frame,frame->buf);
    for (int i = 0; i < FF_ARRAY_ELEMS(frame->buf); i++){
        
        tf_AVBuffer *ref = nullptr;
        if (frame->buf[i]) {
            ref = (tf_AVBuffer*)frame->buf[i]->buffer;
            
            if (ref) std::cout<<"buf "<<ref<<" ref: "<<ref->refcount<<std::endl;
        }
    }
    TFMPDLOG_C("\n---------%s-----------\n",tag);
}


inline void freePacket(AVPacket **pkt){
    av_packet_free(pkt);
}

inline void freeFrame(AVFrame **frame){
    av_frame_free(frame);
}

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
    
    shouldDecode = true;
    
    pktBuffer.valueFreeFunc = freePacket;
    frameBuffer.valueFreeFunc = freeFrame;
    
    RecycleBuffer<TFMPMediaType> intBuffer;
    TFMPMediaType a;
    intBuffer.getOut(&a);
    
    return true;
}

void Decoder::startDecode(){
    pthread_create(&decodeThread, NULL, decodeLoop, this);
    pthread_detach(decodeThread);
}

void Decoder::stopDecode(){
    shouldDecode = false;
}

void Decoder::freeResources(){
    
    if (!shouldDecode) shouldDecode = false;
    
    frameBuffer.signalAllBlock();
    pktBuffer.signalAllBlock();
    while (isDecoding) {
        av_usleep(10000); //0.01s
        TFMPDLOG_C("wait one decode loop end.[%d]\n",steamIndex);
    }
    frameBuffer.clear();
    pktBuffer.clear();
    
    if (codecCtx) avcodec_free_context(&codecCtx);
    fmtCtx = nullptr;
}

void Decoder::decodePacket(AVPacket *packet){
    
    AVPacket *refPacket = av_packet_alloc();
    int retval = av_packet_ref(refPacket, packet);
    TFCheckRetval("av_packet_ref")
    
    pktBuffer.blockInsert(refPacket);
}

void *Decoder::decodeLoop(void *context){
    
    Decoder *decoder = (Decoder *)context;
    
    AVPacket *pkt = nullptr;
    AVFrame *frame = nullptr;
    
    std::vector<AVFrame *> frameArr;
    
    while (decoder->shouldDecode) {
        
        decoder->isDecoding = true;
        
        decoder->pktBuffer.blockGetOut(&pkt);
        
        int retval = avcodec_send_packet(decoder->codecCtx, pkt);
        if (retval < 0) {
            TFCheckRetval("avcodec send packet");
            continue;
        }
        
        if (decoder->type == AVMEDIA_TYPE_AUDIO) { //may many frames in one packet.
            
            while (retval == 0) {
                
                frame = av_frame_alloc();
                retval = avcodec_receive_frame(decoder->codecCtx, frame);
                if (retval == AVERROR(EAGAIN)) {
                    break;
                }
                
                if (retval != 0 && retval != AVERROR_EOF) {
                    TFCheckRetval("avcodec receive frame");
                    continue;
                }
                if (frame->extended_data == nullptr) {
                    printf("audio frame data is null\n");
                    continue;
                }
                
                if (decoder->shouldDecode) decoder->frameBuffer.blockInsert(frame);
            }
        }else{
            AVCodecContext *a;
            a->internal
            frame = av_frame_alloc();
            retval = avcodec_receive_frame(decoder->codecCtx, frame);
//            logBufs(frame, "first out: ");
//            if (decoder->shouldDecode) decoder->frameBuffer.blockInsert(frame);
            
            if (retval != 0) {
                TFCheckRetval("avcodec receive frame");
                continue;
            }
            
            if (frame->extended_data == nullptr) {
                printf("audio frame data is null\n");
                continue;
            }
            
            frameArr.push_back(frame);
            av_usleep(10000);
            if (frameArr.size() > 100) {
                break;
            }
        }
       if (pkt != nullptr)  av_packet_unref(pkt);
    }
    
    
    
    TFMPDLOG_C("start free!------------\n");
    for (auto iter = frameArr.begin(); iter != frameArr.end(); iter++) {
        AVFrame *frame = *iter;
        logBufs(frame, "TWO");
        for (int i = 0; i < FF_ARRAY_ELEMS(frame->buf); i++){
            
            tf_AVBuffer *ref = nullptr;
            if (frame->buf[i]) {
                ref = (tf_AVBuffer*)frame->buf[i]->buffer;
                
//                if (ref) std::cout<<"1:buf "<<ref<<" ref: "<<ref->refcount<<std::endl;
            }
//            av_buffer_unref(&frame->buf[i]);
//            if (ref) atomic_fetch_add_explicit(&ref->refcount, (unsigned int)-1, std::memory_order_acq_rel);
            
            if (ref) {

                //                av_free(ref->data);

                auto fetch = atomic_fetch_add_explicit(&ref->refcount, (unsigned int)-1, std::memory_order_acq_rel);
//                if (ref) std::cout<<"2:buf "<<ref<<" ref: "<<ref->refcount<<" fetch: "<<fetch<<std::endl;

                tf_BufferPoolEntry *buf = (tf_BufferPoolEntry*)ref->opaque;
                tf_AVBufferPool *pool = buf->pool;

               // atomic_fetch_add_explicit(&pool->refcount, (unsigned int)1, std::memory_order_acq_rel);
                std::cout<<"pool: "<<pool<<" ref1: "<<pool->refcount<<std::endl;

                if (fetch == 1) {
                    ref->free(ref->opaque, ref->data);
                    av_freep(&ref);
//                    av_free(ref->data);
                    TFMPDLOG_C("free buf\n");
                }
//                if (atomic_fetch_add_explicit(&pool->refcount, (unsigned int)-1, std::memory_order_acq_rel) == 1){
//                    TFMPDLOG_C("pool free\n");
//                }
                std::cout<<"pool: "<<pool<<" ref2: "<<pool->refcount<<std::endl;
                
                
            }
        }
        
    }
    
    decoder->isDecoding = false;
    
    return 0;
}


