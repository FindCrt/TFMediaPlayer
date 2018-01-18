//
//  TFOPGLESDisplayView.h
//  OPGLES_iOS
//
//  Created by wei shi on 2017/7/12.
//  Copyright © 2017年 wei shi. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "TFGLView.h"
#import "TFMPAVFormat.h"

@interface TFOPGLESDisplayView : TFGLView

-(void)displayFrameBuffer:(TFMPVideoFrameBuffer *)frameBuf;

/** calculate content frame base on source video frame size and content mode */
-(void)calculateContentFrame:(CGSize)sourceSize;


@end
