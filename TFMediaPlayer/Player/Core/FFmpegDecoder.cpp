//
//  FFmpegDecoder.cpp
//  TFMediaPlayer
//
//  Created by shiwei on 2018/11/5.
//  Copyright © 2018年 shiwei. All rights reserved.
//

#include "FFmpegDecoder.hpp"

using namespace tfmpcore;

TFMPVideoFrameBuffer * FFmpegDecoder::displayBufferFromFrame(TFMPFrame *tfmpFrame){
    TFMPVideoFrameBuffer *displayFrame = new TFMPVideoFrameBuffer();
    
    AVFrame *frame = tfmpFrame->frame;
    displayFrame->width = frame->width;
    //TODO: when should i cut one line of data to avoid the green data-empty zone in bottom?
    displayFrame->height = frame->height-1;
    
    for (int i = 0; i<AV_NUM_DATA_POINTERS; i++) {
        
        displayFrame->pixels[i] = frame->data[i]+frame->linesize[i];
        displayFrame->linesize[i] = frame->linesize[i];
    }
    
    //TODO: unsupport format
    if (frame->format == AV_PIX_FMT_YUV420P) {
        displayFrame->format = TFMP_VIDEO_PIX_FMT_YUV420P;
    }else if (frame->format == AV_PIX_FMT_NV12){
        displayFrame->format = TFMP_VIDEO_PIX_FMT_NV12;
    }else if (frame->format == AV_PIX_FMT_NV21){
        displayFrame->format = TFMP_VIDEO_PIX_FMT_NV21;
    }else if (frame->format == AV_PIX_FMT_RGB32){
        displayFrame->format = TFMP_VIDEO_PIX_FMT_RGB32;
    }
    
    return displayFrame;
}

TFMPFrame * FFmpegDecoder::tfmpFrameFromAVFrame(AVFrame *frame, bool isAudio, int serial){
    TFMPFrame *tfmpFrame = new TFMPFrame();
    
    tfmpFrame->serial = serial;
    tfmpFrame->frame  = frame;
    tfmpFrame->type = isAudio ? TFMPFrameTypeAudio:TFMPFrameTypeVideo;
    tfmpFrame->freeFrameFunc = FFmpegDecoder::freeFrame;
    tfmpFrame->pts = frame->pts;
    tfmpFrame->displayBuffer = displayBufferFromFrame(tfmpFrame);
    
    return tfmpFrame;
}

bool FFmpegDecoder::prepareDecode(){
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
    
    return true;
}

void * FFmpegDecoder::decodeLoop(void *context){
    
    FFmpegDecoder *decoder = (FFmpegDecoder *)context;
    
    TFMPPacket packet = {0, nullptr};
    bool packetPending = false;
    AVFrame *frame = nullptr;
    
    string name = decoder->name;
    while (decoder->shouldDecode) {
        
        //TODO: AVERROR_EOF
        int retval = AVERROR(EAGAIN);
        
        do {
            frame = av_frame_alloc();
            retval = avcodec_receive_frame(decoder->codecCtx, frame);
            
            if (retval == AVERROR(EAGAIN)){
                av_frame_free(&frame);
                break;
            }else if (retval != 0){
                av_frame_free(&frame);
            }
        } while (retval != 0);
        
        
        int loopCount = 0;
        do {
            loopCount++;
            if (packetPending) {  //上一轮packet有遗留，就先不取
                packetPending = false;
            }else{
                if (packet.pkt) av_packet_free(&(packet.pkt));
                decoder->pktBuffer.blockGetOut(&packet);
                if (packet.pkt == nullptr) break;
            }
        } while (packet.serial != decoder->serial);
        
        if (loopCount > 1) {  //经历serial变化的阶段，之前的packet不可用
            avcodec_flush_buffers(decoder->codecCtx);
            if (frame) av_frame_free(&frame);
        }else if (frame) {
            //这一句和上面while判断之间，decoder的serial可能会变，但packet的serial不会变
            //没变时，两个值是一样的；变了，这个frame是更早的packet的数据，它的serial肯定是跟packet相同而不是跟decoder相同
            decoder->frameBuffer.blockInsert(tfmpFrameFromAVFrame(frame, true, packet.serial));
        }
        
        retval = avcodec_send_packet(decoder->codecCtx, packet.pkt);
        
        if (retval==AVERROR(EAGAIN)) { //现在接收不了数据，要先取frame
            packetPending = true;
        }else if (retval != 0 && packet.pkt){
            av_packet_free(&(packet.pkt));
        }
    }
    
    return 0;
}
