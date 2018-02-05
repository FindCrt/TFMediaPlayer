//
//  FFmpegInternalDebug.h
//  TFMediaPlayer
//
//  Created by shiwei on 18/2/5.
//  Copyright © 2018年 shiwei. All rights reserved.
//

#ifndef FFmpegInternalDebug_h
#define FFmpegInternalDebug_h

#include "TFMPDebugFuncs.h"

/*** There are some internal structs of FFmpeg such as AVBuffer, and some function for tracing the change of internal status such as AVBuffer.refcount. Those are useful for debuging memory leak. ***/

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


#define AVMutex pthread_mutex_t
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
    TFMPDLOG_C("frame: %x[%lld] buf:%x \n",frame,frame->pts,frame->buf);
    for (int i = 0; i < FF_ARRAY_ELEMS(frame->buf); i++){
        
        tf_AVBuffer *ref = nullptr;
        if (frame->buf[i]) {
            ref = (tf_AVBuffer*)frame->buf[i]->buffer;
            
            if (ref) std::cout<<"buf "<<ref<<" ref: "<<ref->refcount<<std::endl;
        }
    }
    TFMPDLOG_C("\n---------%s-----------\n",tag);
}

inline void logAVBufferPool(AVBufferRef *ref, bool unref){
    tf_AVBuffer *buf = nullptr;
    if (ref) {
        buf = (tf_AVBuffer*)ref->buffer;
    }
    
    if (buf != nullptr) {
        
        std::atomic_uint fetch;
        if (unref) {
            fetch = atomic_fetch_add_explicit(&buf->refcount, (unsigned int)-1, std::memory_order_acq_rel);
        }
        tf_BufferPoolEntry *poolEn = (tf_BufferPoolEntry*)buf->opaque;
        tf_AVBufferPool *pool = poolEn->pool;
        
        std::cout<<"buf: "<<buf<<" ref1: "<<buf->refcount<<std::endl;
        std::cout<<"pool: "<<pool<<" ref1: "<<pool->refcount<<std::endl;
        
        if (fetch == 1) {
            buf->free(buf->opaque, buf->data);
            av_freep(&ref);
            TFMPDLOG_C("free buf\n");
        }
    }
    
    
}


#endif /* FFmpegInternalDebug_h */
