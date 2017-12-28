//
//  PlayController.hpp
//  TFMediaPlayer
//
//  Created by shiwei on 17/12/25.
//  Copyright © 2017年 shiwei. All rights reserved.
//

#ifndef PlayController_hpp
#define PlayController_hpp

#include <stdio.h>
#include <string>

namespace tfmpcore {
    class PlayController{
        
        std::string _mediaPath;
        
    public:
        bool connectAndOpenMedia(std::string mediaPath);
        
        void play();
        void pause();
        void stop();
    };
}

#endif /* PlayController_hpp */
