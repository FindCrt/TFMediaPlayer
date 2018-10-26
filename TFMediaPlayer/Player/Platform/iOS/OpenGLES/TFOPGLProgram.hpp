//
//  TFOPGLProgram.hpp
//  triangle
//
//  Created by wei shi on 2017/7/3.
//  Copyright © 2017年 wei shi. All rights reserved.
//

#ifndef TFOPGLProgram_hpp
#define TFOPGLProgram_hpp

#include <stdlib.h>
#include <string>
#include <TargetConditionals.h>

#if TARGET_OS_OSX

#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "SOIL.h"

#elif TARGET_OS_IOS

#import "TFMPUtilities.h"
#if KAdoptES2
#include <OpenGLES/ES2/gl.h>
#else
#include <OpenGLES/ES3/gl.h>
#endif

#endif

#if USING_3D
#include "glm.hpp"
#include "matrix_transform.hpp"
#include "type_ptr.hpp"
#endif

#ifndef TFInfoLogLen
#define TFInfoLogLen    256
#endif

#define TFUndefinedShader       -1
#define TFUndefinedProgram      -1

class TFOPGLProgram{
    
    GLuint _vertexShader = TFUndefinedShader;
    GLuint _fragmentShader = TFUndefinedShader;
    
    GLuint _program = TFUndefinedProgram;
    
public:
    TFOPGLProgram(){};
    TFOPGLProgram(std::string vertexShaderPath, std::string fragmentShaderPath);
    TFOPGLProgram(const char *vertexShaderSource, const char *fragmentShaderSource);
    
    bool loadFailed(){
        return _program == TFUndefinedProgram;
    }
    /**
     load shader source from the path and bind it to a new shader of the type.
     
     @param type the shader type
     @param path the shader path
     @param error  load source or compile error
     @return the shader loaded recently
     */
    bool loadShaderWithPath(GLenum type, std::string path, char **error);
    bool loadShaderWithPathAutoLog(GLenum type, std::string path);
    
    bool loadShaderWithSourceString(GLenum type, const char *source, char**error);
    
    bool attachShadersAndlinkProgram();
    
    /***** align uniforms *****/
    void setFloat(std::string name, GLfloat value);
    void setTexture(std::string name, GLenum textureType, GLuint texture, GLuint textureUnit);
    
#if USING_3D
    void setVec3f(std::string name, glm::vec3 vector, bool needNormalize = false);
    void setVec3f(std::string name, GLfloat val1, GLfloat val2, GLfloat val3, bool needNormalize = false);
    void setMatrix(std::string name, glm::mat4 matrix);
#endif
    
    bool use();
};

#endif /* TFOPGLProgram_hpp */
