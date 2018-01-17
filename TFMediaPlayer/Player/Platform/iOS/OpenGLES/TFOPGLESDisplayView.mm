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

//texture对齐和裁剪 http://www.zwqxin.com/archives/opengl/opengl-api-memorandum-2.html

#define TFReallocRenderIfLayerSizeChanged   1

#pragma mark - shaders

const GLchar *TFVideoDisplay_common_vs ="               \n\
#version 300 es                                         \n\
                                                        \n\
layout (location = 0) in highp vec3 position;           \n\
layout (location = 1) in highp vec2 inTexcoord;         \n\
                                                        \n\
out highp vec2 texcoord;                                \n\
                                                        \n\
void main()                                             \n\
{                                                       \n\
gl_Position = vec4(position, 1.0);                      \n\
texcoord = inTexcoord;                                  \n\
}                                                       \n\
";

const GLchar *TFVideoDisplay_yuv420_fs ="               \n\
#version 300 es                                         \n\
precision highp float;                                  \n\
                                                        \n\
in vec2 texcoord;                                       \n\
out vec4 FragColor;                                     \n\
uniform lowp sampler2D yPlaneTex;                       \n\
uniform lowp sampler2D uPlaneTex;                       \n\
uniform lowp sampler2D vPlaneTex;                       \n\
                                                        \n\
void main()                                             \n\
{                                                       \n\
    // (1) y - 16 (2) rgb * 1.164                       \n\
    vec3 yuv;                                           \n\
    yuv.x = texture(yPlaneTex, texcoord).r;             \n\
    yuv.y = texture(uPlaneTex, texcoord).r - 0.5f;      \n\
    yuv.z = texture(vPlaneTex, texcoord).r - 0.5f;      \n\
                                                        \n\
    mat3 trans = mat3(1, 1 ,1,                          \n\
                      0, -0.34414, 1.772,               \n\
                      1.402, -0.71414, 0                \n\
                      );                                \n\
                                                        \n\
    FragColor = vec4(trans*yuv, 1.0);                   \n\
}                                                       \n\
";

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
    //[super didMoveToSuperview];
    
    if (!self.context) {
        [self setupOpenGLContext];
    }
}

-(void)layoutSubviews{
    [super layoutSubviews];
    
    //If context has setuped and layer's size has changed, realloc renderBuffer.
    if (self.context && !CGSizeEqualToSize(self.layer.frame.size, self.bufferSize)) {
#if TFReallocRenderIfLayerSizeChanged
        _needReallocRenderBuffer = YES;
#else
        //self.layer.contentsScale = [UIScreen mainScreen].scale * ()
#endif
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
    
//    NSString *vertexPath = [[NSBundle mainBundle] pathForResource:@"frameDisplay" ofType:@"vs"];
//    NSString *fragmentPath = [[NSBundle mainBundle] pathForResource:@"frameDisplay" ofType:@"fs"];
    //_frameProgram = new TFOPGLProgram(std::string([vertexPath UTF8String]), std::string([fragmentPath UTF8String]));
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

-(void)displayFrameBuffer:(TFMPVideoFrameBuffer *)frameBuf{
    
    if (self.appIsUnactive) {
        return;
    }
    
//    if (!self.context) {
//        [self setupOpenGLContext];
//    }
    EAGLContext *preContex = [EAGLContext currentContext];
    [EAGLContext setCurrentContext:self.context];
    
    if (_needReallocRenderBuffer) {
        [self reallocRenderBuffer];
        
        _needReallocRenderBuffer = NO;
    }
    
    if (!_renderConfiged) {
        [self configRenderData];
    }
    
    float width = frameBuf->width;
    float height = frameBuf->height;
    
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
    glGenerateMipmap(GL_TEXTURE_2D);
    
    
    glPixelStorei(GL_UNPACK_ROW_LENGTH, linesize[1]);
    
    glBindTexture(GL_TEXTURE_2D, textures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width/2.0, height/2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, frameBuf->pixels[1]);
    glGenerateMipmap(GL_TEXTURE_2D);
    
    
    glPixelStorei(GL_UNPACK_ROW_LENGTH, linesize[2]);
    
    glBindTexture(GL_TEXTURE_2D, textures[2]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width/2.0, height/2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, frameBuf->pixels[2]);
    glGenerateMipmap(GL_TEXTURE_2D);
}

inline void useTexturesForProgram_YUV420P(TFOPGLProgram *program, GLuint *textures){
    program->setTexture("yPlaneTex", GL_TEXTURE_2D, textures[0], 0);
    program->setTexture("uPlaneTex", GL_TEXTURE_2D, textures[1], 1);
    program->setTexture("vPlaneTex", GL_TEXTURE_2D, textures[2], 2);
}

#pragma mark -

@end
