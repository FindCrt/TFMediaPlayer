//
//  TFGLView.m
//  OPGLES_iOS
//
//  Created by wei shi on 2017/7/12.
//  Copyright © 2017年 wei shi. All rights reserved.
//

#import "TFGLView.h"

@interface TFGLView (){
    GLuint _depthRenderbuffer;
}

@end

@implementation TFGLView

+(Class)layerClass{
    return [CAEAGLLayer class];
}

-(BOOL)setupOpenGLContext{
    _renderLayer = (CAEAGLLayer *)self.layer;
    _renderLayer.opaque = YES;
    _renderLayer.contentsScale = [UIScreen mainScreen].scale;
    _renderLayer.drawableProperties = [NSDictionary dictionaryWithObjectsAndKeys:
                                       [NSNumber numberWithBool:NO], kEAGLDrawablePropertyRetainedBacking,
                                       kEAGLColorFormatRGBA8, kEAGLDrawablePropertyColorFormat,
                                       nil];
    
    _context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
//    _context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
    if (!_context) {
        NSLog(@"alloc EAGLContext failed!");
        return false;
    }
    EAGLContext *preContex = [EAGLContext currentContext];
    if (![EAGLContext setCurrentContext:_context]) {
        NSLog(@"set current EAGLContext failed!");
        return false;
    }
    [self setupFrameBuffer];
    
    [EAGLContext setCurrentContext:preContex];
    
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(catchAppResignActive) name:UIApplicationWillResignActiveNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(catchAppBecomeActive) name:UIApplicationDidBecomeActiveNotification object:nil];
    
    
    
    return true;
}

-(void)catchAppResignActive{
    _appIsUnactive = YES;
}

-(void)catchAppBecomeActive{
    _appIsUnactive = NO;
}

-(void)setupFrameBuffer{
    glGenBuffers(1, &_frameBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, _frameBuffer);
    
    glGenRenderbuffers(1, &_colorBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, _colorBuffer);
    [_context renderbufferStorage:GL_RENDERBUFFER fromDrawable:_renderLayer];
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _colorBuffer);
    
    GLint width,height;
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &width);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &height);
    
    _bufferSize.width = width;
    _bufferSize.height = height;
    
    if (_needDepthBuffer) {
        GLuint depthRenderbuffer;
        glGenRenderbuffers(1, &depthRenderbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, depthRenderbuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
        //[_context renderbufferStorage:GL_RENDERBUFFER fromDrawable:_layer];
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRenderbuffer);
    }
    
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER) ;
    if(status != GL_FRAMEBUFFER_COMPLETE) {
        NSLog(@"failed to make complete framebuffer object %x", status);
    }
}

-(void)reallocRenderBuffer{
    
    glBindRenderbuffer(GL_RENDERBUFFER, _colorBuffer);
    
    [_context renderbufferStorage:GL_RENDERBUFFER fromDrawable:_renderLayer];
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _colorBuffer);
    
    GLint width,height;
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &width);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &height);
    
    _bufferSize.width = width;
    _bufferSize.height = height;
    
    if (_needDepthBuffer) {
        glGenRenderbuffers(1, &_depthRenderbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, _depthRenderbuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
        //[_context renderbufferStorage:GL_RENDERBUFFER fromDrawable:_layer];
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, _depthRenderbuffer);
    }
}

-(void)dealloc{
    if (_colorBuffer) {
        glDeleteRenderbuffers(1, &_colorBuffer);
    }
    
    if (_depthRenderbuffer) {
        glDeleteRenderbuffers(1, &_depthRenderbuffer);
    }
    
    if (_frameBuffer) {
        glDeleteBuffers(1, &_frameBuffer);
    }
}

@end
