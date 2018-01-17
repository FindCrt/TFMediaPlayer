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
    //_context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
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

-(void)calculateContentFrame:(CGSize)sourceSize{
    
    CGRect contentFrame;
    CGFloat width = self.bufferSize.width, height = self.bufferSize.height;
    
    if (self.contentMode == UIViewContentModeScaleAspectFit) {
        
        CGFloat scaledWidth = sourceSize.width * height/sourceSize.height;

        if (scaledWidth < width) { //height is relatively larger .
            contentFrame = CGRectMake((width-scaledWidth)/2.0, 0, scaledWidth, height);
        }else{
            CGFloat scaledHeight = sourceSize.height * width/sourceSize.width;
            contentFrame = CGRectMake(0, (height-scaledHeight)/2.0, width, scaledHeight);
        }
    }else if (self.contentMode == UIViewContentModeScaleAspectFill){
        
        CGFloat scaledWidth = sourceSize.width * height/sourceSize.height;
        
        if (scaledWidth > width) { //height is relatively larger .
            contentFrame = CGRectMake((width-scaledWidth)/2.0, 0, scaledWidth, height);
        }else{
            CGFloat scaledHeight = sourceSize.height * width/sourceSize.width;
            contentFrame = CGRectMake(0, (scaledHeight-height)/2.0, width, scaledHeight);
        }
    }else if (self.contentMode == UIViewContentModeScaleToFill){
        contentFrame = CGRectMake(0, 0, width, height);
    }else if (self.contentMode == UIViewContentModeCenter){
        contentFrame = CGRectMake((width-sourceSize.width)/2.0, (height-sourceSize.height)/2.0, sourceSize.width, sourceSize.height);
    }else if (self.contentMode == UIViewContentModeTop){
        contentFrame = CGRectMake((width-sourceSize.width)/2.0, 0, sourceSize.width, sourceSize.height);
    }else if (self.contentMode == UIViewContentModeBottom){
        contentFrame = CGRectMake((width-sourceSize.width)/2.0, height-sourceSize.height, sourceSize.width, sourceSize.height);
    }else if (self.contentMode == UIViewContentModeLeft){
        contentFrame = CGRectMake(0, (height-sourceSize.height)/2.0, sourceSize.width, sourceSize.height);
    }else if (self.contentMode == UIViewContentModeRight){
        contentFrame = CGRectMake(width-sourceSize.width, (height-sourceSize.height)/2.0, sourceSize.width, sourceSize.height);
    }else if (self.contentMode == UIViewContentModeTopLeft){
        contentFrame = CGRectMake(0, 0, sourceSize.width, sourceSize.height);
    }else if (self.contentMode == UIViewContentModeTopRight){
        contentFrame = CGRectMake(width-sourceSize.width, 0, sourceSize.width, sourceSize.height);
    }else if (self.contentMode == UIViewContentModeBottomLeft){
        contentFrame = CGRectMake(0, height-sourceSize.height, sourceSize.width, sourceSize.height);
    }else if (self.contentMode == UIViewContentModeBottomRight){
        contentFrame = CGRectMake(width-sourceSize.width, height-sourceSize.height, sourceSize.width, sourceSize.height);
    }
    
    //opengl's y axis is inverse with UIKit's y asix.
    glViewport(contentFrame.origin.x, height - CGRectGetMaxY(contentFrame), contentFrame.size.width, contentFrame.size.height);
}

-(void)dealloc{
    if (_colorBuffer) {
        glDeleteRenderbuffers(1, &_colorBuffer);
    }
    
    if (_depthRenderbuffer) {
        glDeleteRenderbuffers(1, &_depthRenderbuffer);
    }
    
    if (_frameBuffer) {
        glDeleteFramebuffers(1, &_frameBuffer);
    }
}

@end
