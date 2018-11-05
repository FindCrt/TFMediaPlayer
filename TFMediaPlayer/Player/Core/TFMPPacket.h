//
//  TFMPPacket.h
//  TFMediaPlayer
//
//  Created by shiwei on 2018/10/26.
//  Copyright © 2018年 shiwei. All rights reserved.
//

#ifndef TFMPPacket_h
#define TFMPPacket_h

extern "C"{
#include <libavformat/avformat.h>
}

namespace tfmpcore {
    struct TFMPPacket{
        int serial;
        AVPacket *pkt;
        
        TFMPPacket(){
            serial = 0;
            pkt = nullptr;
        };
        TFMPPacket(int serial, AVPacket *pkt):serial(serial),pkt(pkt){};
    };
}

#endif /* TFMPPacket_h */
