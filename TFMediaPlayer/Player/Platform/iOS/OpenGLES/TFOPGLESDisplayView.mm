//
//  TFOPGLESDisplayView.m
//  OPGLES_iOS
//
//  Created by wei shi on 2017/7/12.
//  Copyright © 2017年 wei shi. All rights reserved.
//

#import "TFOPGLESDisplayView.h"
#import <OpenGLES/ES3/gl.h>
#import "TFOPGLProgram.hpp"
#import "TFMPDebugFuncs.h"
#import "TFMPUtilities.h"

//texture对齐和裁剪 http://www.zwqxin.com/archives/opengl/opengl-api-memorandum-2.html

#pragma mark - shaders

//shader代码开头带#，但放在这里会破坏宏
const GLchar *TFVideoDisplay_common_vs = TFGLShaderSource_sharp
(
 version 300 es
 
 layout (location = 0) in mediump vec3 position;
 layout (location = 1) in mediump vec2 inTexcoord;
 
 out mediump vec2 texcoord;
 
 void main()
 {
     gl_Position = vec4(position, 1.0);
     texcoord = inTexcoord;
 }
);

const GLchar *TFVideoDisplay_yuv420_fs = TFGLShaderSource_sharp
(
 version 300 es
 
 precision mediump float;
 
 in vec2 texcoord;
 
 out vec4 FragColor;
 
 uniform sampler2D yPlaneTex;
 uniform sampler2D uPlaneTex;
 uniform sampler2D vPlaneTex;
 
 
 void main()
{
    // (1) y - 16 (2) rgb * 1.164
    vec3 yuv;
    yuv.x = texture(yPlaneTex, texcoord).r;
    yuv.y = texture(uPlaneTex, texcoord).r - 0.5f;
    yuv.z = texture(vPlaneTex, texcoord).r - 0.5f;
    
    mat3 trans = mat3(1, 1 ,1,
                      0, -0.34414, 1.772,
                      1.402, -0.71414, 0
                      );
    
    FragColor = vec4(trans*yuv, 1.0);
}
 );


#pragma mark -

#define TFMAX_TEXTURE_COUNT     3

@interface TFOPGLESDisplayView (){
    
    TFOPGLProgram *_frameProgram;
    GLuint VAO;
    GLuint VBO;
    GLuint textures[TFMAX_TEXTURE_COUNT];
    
    BOOL _renderConfiged;
    
    BOOL _needReallocRenderBuffer;
    
    CGSize _lastFrameSize;
}

@end

@implementation TFOPGLESDisplayView

-(instancetype)initWithFrame:(CGRect)frame{
    if (self = [super initWithFrame:frame]) {
        self.contentMode = UIViewContentModeScaleAspectFit;
    }
    
    return self;
}

-(void)didMoveToSuperview{
    if (!self.context) {
        [self setupOpenGLContext];
    }
}

-(void)layoutSubviews{
    [super layoutSubviews];
    
    //If context has setuped and layer's size has changed, realloc renderBuffer.
    if (self.context && !CGSizeEqualToSize(CGSizeMultiply(self.layer.frame.size, self.layer.contentsScale), self.bufferSize)) {
        TFGLSwitchContextToDo(self.context, {
            [self reallocRenderBuffer];
            [self calculateContentFrame:_lastFrameSize];
        });
    }
}

-(void)setContentMode:(UIViewContentMode)contentMode{
    if (self.contentMode != contentMode) {
        [super setContentMode:contentMode];
        
        [self calculateContentFrame:_lastFrameSize];
    }
}

-(BOOL)configRenderData{
    if (_renderConfiged) {
        return true;
    }
    
    GLfloat vertices[] = {
        -1.0f, 1.0f, 0.0f, 0.0f, 0.0f,  //left top
        -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, //left bottom
        1.0f, 1.0f, 0.0f, 1.0f, 0.0f,   //right top
        1.0f, -1.0f, 0.0f, 1.0f, 1.0f,  //right bottom
    };
    _frameProgram = new TFOPGLProgram(TFVideoDisplay_common_vs, TFVideoDisplay_yuv420_fs);
    
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5*sizeof(GL_FLOAT), 0);
    glEnableVertexAttribArray(0);
    
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5*sizeof(GL_FLOAT), (void*)(3*(sizeof(GL_FLOAT))));
    glEnableVertexAttribArray(1);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    
    
    //gen textures
    glGenTextures(TFMAX_TEXTURE_COUNT, textures);
    for (int i = 0; i<TFMAX_TEXTURE_COUNT; i++) {
        glBindTexture(GL_TEXTURE_2D, textures[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    }
    _renderConfiged = YES;
    
    return YES;
}

//-(void)renderImageBuffer:(TFImageBuffer *)imageBuf{
//    
//    [self configRenderData];
//    
//    float width = imageBuf->width;
//    float height = imageBuf->height;
//    
//    CGSize viewPort;
//    if (width / height > self.bufferSize.width / self.bufferSize.height) {
//        viewPort.width = self.bufferSize.width;
//        viewPort.height = height / width * self.bufferSize.width;
//    }else{
//        viewPort.width = width / height * self.bufferSize.height;
//        viewPort.height = self.bufferSize.height;
//    }
//    
//    glViewport(0, 0, viewPort.width, viewPort.height);
//    
//    
//    glBindTexture(GL_TEXTURE_2D, textures[0]);
//    
//    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, imageBuf->pixels[0]);
//    glGenerateMipmap(GL_TEXTURE_2D);
//    
//    [self rendering];
//}

-(void)displayPixelBuffer:(CVPixelBufferRef)pixelBuffer{
    
    if (CVPixelBufferGetBaseAddress(pixelBuffer) == nil) {
        return;
    }
    
    TFMPVideoFrameBuffer frame = [self TFMPFrameBufferFromPixelBuffer:pixelBuffer];
    CVPixelBufferLockBaseAddress(pixelBuffer, 0);
    [self displayFrameBuffer:&frame];
    CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
    CVPixelBufferRelease(pixelBuffer);
}

-(TFMPVideoFrameBuffer)TFMPFrameBufferFromPixelBuffer:(CVPixelBufferRef)pixelBuffer{
    TFMPVideoFrameBuffer frame;
    frame.width = (int)CVPixelBufferGetWidth(pixelBuffer);
    frame.height = (int)CVPixelBufferGetHeight(pixelBuffer);
    
    OSType pixelType = CVPixelBufferGetPixelFormatType(pixelBuffer);
    if (pixelType == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange ||
        pixelType == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange){
        
        uint8_t *buffer = (uint8_t*)CVPixelBufferGetBaseAddress(pixelBuffer);
        uint8_t *yuv420p = (uint8_t*)malloc(frame.width*frame.height*3/2.0f);
        yuv420sp_to_yuv420p(buffer, yuv420p, frame.width, frame.height);
        
        frame.format = TFMP_VIDEO_PIX_FMT_YUV420P;
        frame.planes = 3;
        uint32_t ysize = frame.width*frame.height;
        frame.pixels[0] = yuv420p;
        frame.pixels[1] = yuv420p+ysize;
        frame.pixels[2] = yuv420p+(5/4)*ysize;
        frame.linesize[0] = frame.width;
        frame.linesize[1] = frame.width/2;
        frame.linesize[2] = frame.width/2;
        
    }else{
        frame.format = TFMP_VIDEO_PIX_FMT_YUV420P;
        frame.planes = 3;
        
        frame.pixels[0] = (uint8_t*)CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 0);
        frame.pixels[1] = (uint8_t*)CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 1);;
        frame.pixels[2] = (uint8_t*)CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 2);
        frame.linesize[0] = frame.width;
        frame.linesize[1] = frame.width/2;
        frame.linesize[2] = frame.width/2;
    }
    
    return frame;
}

-(void)displayFrameBuffer:(TFMPVideoFrameBuffer *)frameBuf{
    
#if DisableRenderVideo
    return;
#endif
    if (self.appIsUnactive) {
        return;
    }
    
//    if (!self.context) {
//        [self setupOpenGLContext];
//    }
    EAGLContext *preContex = [EAGLContext currentContext];
    [EAGLContext setCurrentContext:self.context];
    
    if (_needReallocRenderBuffer) {
//        [self reallocRenderBuffer];
        
        _needReallocRenderBuffer = NO;
    }
    
    if (!_renderConfiged) {
        [self configRenderData];
    }
    
    float width = frameBuf->width;
    float height = frameBuf->height;
    
    //TODO: view mode must be runed on main thread
    if (width != _lastFrameSize.width || height != _lastFrameSize.height) {
        [self calculateContentFrame:CGSizeMake(width, height)];
        _lastFrameSize = CGSizeMake(width, height);
    }

    if (frameBuf->format == TFMP_VIDEO_PIX_FMT_YUV420P) {
        genTextures_YUV420P(frameBuf, textures, width, height, frameBuf->linesize);
    }
    
    [self rendering:frameBuf->format];
    
    [EAGLContext setCurrentContext:preContex];
}

-(void)rendering:(TFMPVideoPixelFormat)format{
    
    glBindFramebuffer(GL_FRAMEBUFFER, self.frameBuffer);
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    
    _frameProgram->use();
    
    if (format == TFMP_VIDEO_PIX_FMT_YUV420P) {
        useTexturesForProgram_YUV420P(_frameProgram, textures);
    }
    
    glBindVertexArray(VAO);
    
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    
    glBindRenderbuffer(GL_RENDERBUFFER, self.colorBuffer);
    [self.context presentRenderbuffer:GL_RENDERBUFFER];
}

-(void)dealloc{
    
    if (_renderConfiged) {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        
        glDeleteTextures(TFMAX_TEXTURE_COUNT, textures);
    }
}

#pragma mark - display different format

inline void genTextures_YUV420P(TFMPVideoFrameBuffer *frameBuf, GLuint *textures, int width, int height, int *linesize){
    //yuv420p has 3 planes: y u v. U plane and v plane have half width and height of y plane.
    
    glPixelStorei(GL_UNPACK_ROW_LENGTH, linesize[0]);  //linesize may isn't equal to width.
    
    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width, height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, frameBuf->pixels[0]);
    
    
    glPixelStorei(GL_UNPACK_ROW_LENGTH, linesize[1]);
    
    glBindTexture(GL_TEXTURE_2D, textures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width/2.0, height/2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, frameBuf->pixels[1]);
    
    
    glPixelStorei(GL_UNPACK_ROW_LENGTH, linesize[2]);
    
    glBindTexture(GL_TEXTURE_2D, textures[2]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width/2.0, height/2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, frameBuf->pixels[2]);
    
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}

inline void useTexturesForProgram_YUV420P(TFOPGLProgram *program, GLuint *textures){
    program->setTexture("yPlaneTex", GL_TEXTURE_2D, textures[0], 0);
    program->setTexture("uPlaneTex", GL_TEXTURE_2D, textures[1], 1);
    program->setTexture("vPlaneTex", GL_TEXTURE_2D, textures[2], 2);
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
    }else{
        //其他都按center的模式来显示
        contentFrame = CGRectMake((width-sourceSize.width)/2.0, (height-sourceSize.height)/2.0, sourceSize.width, sourceSize.height);
    }
    
    contentFrame = CGRectMake((int)contentFrame.origin.x, (int)contentFrame.origin.y, (int)contentFrame.size.width, (int)contentFrame.size.height);
    
    //opengl's y axis is inverse with UIKit's y asix.
    TFGLSwitchContextToDo(self.context, {
        glViewport(contentFrame.origin.x, height - CGRectGetMaxY(contentFrame), contentFrame.size.width, contentFrame.size.height);
    });
    
    NSLog(@"contentFrame: %@",NSStringFromCGRect(contentFrame));
}

#pragma mark -

@end
