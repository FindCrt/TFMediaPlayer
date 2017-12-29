//
//  TFOPGLESDisplayView.h
//  OPGLES_iOS
//
//  Created by wei shi on 2017/7/12.
//  Copyright © 2017年 wei shi. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "TFGLView.h"
#import "VideoFormat.h"

@interface TFOPGLESDisplayView : TFGLView

-(void)displayFrameBuffer:(TFMPVideoFrameBuffer *)frameBuf;

@end
