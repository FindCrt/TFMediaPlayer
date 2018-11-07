//
//  VTBDecoder.hpp
//  TFMediaPlayer
//
//  Created by shiwei on 2018/9/28.
//  Copyright © 2018年 shiwei. All rights reserved.
//

#ifndef VTBDecoder_hpp
#define VTBDecoder_hpp

#include <stdio.h>
#include "Decoder.hpp"
#include <VideoToolbox/VideoToolbox.h>

#include "RecycleBuffer.hpp"
#include "TFMPAVFormat.h"
#include "TFMPFrame.h"

using namespace std;

//An video & audio decoder based on VideoToolBox and ffmpeg.
namespace tfmpcore {
    
    class VTBDecoder : public Decoder{
        
        static void *decodeLoop(void *context);
        
        VTDecompressionSessionRef _decodeSession;
        CMFormatDescriptionRef _videoFmtDesc;
        
        uint8_t *_sps;
        uint8_t *_pps;
        uint32_t _spsSize = 0;
        uint32_t _ppsSize = 0;
        
        void decodePacket(TFMPPacket *packet);
        
        void static decodeCallback(void * CM_NULLABLE decompressionOutputRefCon,void * CM_NULLABLE sourceFrameRefCon,OSStatus status,VTDecodeInfoFlags infoFlags,CM_NULLABLE CVImageBufferRef imageBuffer,CMTime presentationTimeStamp,CMTime presentationDuration );
        
        static TFMPVideoFrameBuffer *displayBufferFromPixelBuffer(CVPixelBufferRef pixelBuffer);
        
        inline static void freeFrame(TFMPFrame **frameP){
            TFMPFrame *frame = *frameP;
            
            CVPixelBufferRef pixelBuffer = (CVPixelBufferRef)frame->displayBuffer->opaque;
            CVPixelBufferRelease(pixelBuffer);
            
            delete frame->displayBuffer;
            
            delete frame;
            *frameP = nullptr;
            
            myStateObserver.mark("video free", 1, true);
        }
        
        inline static int frameCompare(TFMPFrame *&frame1, TFMPFrame *&frame2){
            if (frame1->pts < frame2->pts) {
                return -1;
            }else{
                return 1;
            }
        }
        
    public:
        VTBDecoder(){
            decodeLoopFunc = decodeLoop;
        }
        
        virtual bool prepareDecode();
        virtual void stopDecode();
    };
}

#endif /* VTBDecoder_hpp */
