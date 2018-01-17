//
//  TFGLView.h
//  OPGLES_iOS
//
//  Created by wei shi on 2017/7/12.
//  Copyright © 2017年 wei shi. All rights reserved.
//

#import <UIKit/UIKit.h>
#import <OpenGLES/ES3/gl.h>

@interface TFGLView : UIView

@property (nonatomic, assign) BOOL needDepthBuffer;

@property (atomic, assign, readonly) BOOL appIsUnactive;

@property (nonatomic, strong, readonly) EAGLContext *context;

@property (nonatomic, strong, readonly) CAEAGLLayer *renderLayer;

@property (nonatomic, assign, readonly) GLuint frameBuffer;

@property (nonatomic, assign, readonly) GLuint colorBuffer;

@property (nonatomic, assign, readonly) CGSize bufferSize;

-(BOOL)setupOpenGLContext;

//must call super if override it.
-(void)setupFrameBuffer;

-(void)reallocRenderBuffer;

/** calculate content frame base on source video frame size and content mode */
-(void)calculateContentFrame:(CGSize)sourceSize;

@end
