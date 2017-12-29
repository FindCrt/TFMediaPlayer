//
//  TFOPGLProgram.cpp
//  triangle
//
//  Created by wei shi on 2017/7/3.
//  Copyright © 2017年 wei shi. All rights reserved.
//

#include "TFOPGLProgram.hpp"
#include <fstream>

#define ProgramIsUndefinedLog   printf("program is undefined\n");

TFOPGLProgram::TFOPGLProgram(std::string vertexShaderPath, std::string fragmentShaderPath){
    
    loadShaderWithPathAutoLog(GL_VERTEX_SHADER, vertexShaderPath);
    loadShaderWithPathAutoLog(GL_FRAGMENT_SHADER, fragmentShaderPath);
    attachShadersAndlinkProgram();
}

TFOPGLProgram::TFOPGLProgram(const char *vertexShaderSource, const char *fragmentShaderSource){
    char *error = new char[TFInfoLogLen];
    bool succeed = loadShaderWithSourceString(GL_VERTEX_SHADER, vertexShaderSource, &error);
    if (!succeed) {
        printf("%s\n",error);
        return;
    }
    succeed = loadShaderWithSourceString(GL_FRAGMENT_SHADER, fragmentShaderSource, &error);
    if (!succeed) {
        printf("%s\n",error);
        return;
    }
    attachShadersAndlinkProgram();
}

std::string readTxt(std::string file)
{
    std::ifstream infile;
    infile.open(file.data());
    
    std::string s;
    std::string totalText;
    while(getline(infile,s))
    {
        totalText += s + "\n";
    }
    infile.close();
    
    return totalText;
}

bool TFOPGLProgram::loadShaderWithPathAutoLog(GLenum type, std::string path){
    char *error = new char[TFInfoLogLen];
    bool succeed = loadShaderWithPath(type, path, &error);
    
#if DEBUG
    if (!succeed) {
        printf("load shader error,\npath:%s \nerror:%s\n",path.c_str(),error);
    }
    
    free(error);
#endif
    
    return succeed;
}

bool TFOPGLProgram::loadShaderWithPath(GLenum type, std::string path, char **error){
    std::string sourceStr = readTxt(path);
    const GLchar *source = sourceStr.c_str();
    
    return loadShaderWithSourceString(type, source, error);
}

bool TFOPGLProgram::loadShaderWithSourceString(GLenum type, const char *source, char **error){
    GLuint shader = glCreateShader(type);
    if (type == GL_VERTEX_SHADER) {
        _vertexShader = shader;
    }else if (type == GL_FRAGMENT_SHADER){
        _fragmentShader = shader;
    }
    
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    
    GLint succeed = true;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &succeed);
    
#if DEBUG
    if (succeed != GL_TRUE) {
        if (error) {
            glGetShaderInfoLog(shader, TFInfoLogLen, nullptr, *error);
            glDeleteShader(shader);
            
            if (type == GL_VERTEX_SHADER) {
                _vertexShader = TFUndefinedShader;
            }else if (type == GL_FRAGMENT_SHADER){
                _fragmentShader = TFUndefinedShader;
            }
        }
        
        return false;
    }
#endif
    
    return true;
}

bool TFOPGLProgram::attachShadersAndlinkProgram(){
    if (_vertexShader == TFUndefinedShader || _fragmentShader == TFUndefinedShader) {
        printf("vertex shader is undefined or fragment shader is undefined");
        return false;
    }
    _program = glCreateProgram();
    glAttachShader(_program, _vertexShader);
    glAttachShader(_program, _fragmentShader);
    
    glLinkProgram(_program);
    

    GLint succeed = true;
    glGetProgramiv(_program, GL_LINK_STATUS, &succeed);
    
#if DEBUG
    if (!succeed) {
        GLchar *infolog = new char[TFInfoLogLen];
        glGetProgramInfoLog(_program, TFInfoLogLen, nullptr, infolog);
        printf("link program error:\n%s\n",infolog);
        
        free(infolog);
        glDeleteProgram(_program);
        _program = TFUndefinedProgram;
    }
#endif
    
    return succeed;
}

#pragma mark - align uniforms

void TFOPGLProgram::setFloat(std::string name, GLfloat value){
    if (_program == TFUndefinedProgram) {
        ProgramIsUndefinedLog
        return;
    }
    GLint location = glGetUniformLocation(_program, name.c_str());
    if (location >= 0) {
        glUniform1f(location, value);
    }
}

#if USING_3D

void TFOPGLProgram::setVec3f(std::string name, GLfloat val1, GLfloat val2, GLfloat val3, bool needNormalize){
    if (_program == TFUndefinedProgram) {
        ProgramIsUndefinedLog
        return;
    }
    GLint location = glGetUniformLocation(_program, name.c_str());
    if (location >= 0) {
        if (needNormalize) {
            glm::vec3 normalizedVec3 = glm::normalize(glm::vec3(val1, val2, val3));
            glUniform3f(location, normalizedVec3.x, normalizedVec3.y, normalizedVec3.z);
        }else{
            glUniform3f(location, val1, val2, val3);
        }
    }
}

void TFOPGLProgram::setVec3f(std::string name, glm::vec3 vector, bool needNormalize){
    if (_program == TFUndefinedProgram) {
        ProgramIsUndefinedLog
        return;
    }
    GLint location = glGetUniformLocation(_program, name.c_str());
    if (location >= 0) {
        if (needNormalize) {
            vector = glm::normalize(vector);
        }
        glUniform3fv(location,1, glm::value_ptr(vector));
    }
}

void TFOPGLProgram::setMatrix(std::string name, glm::mat4 matrix){
    if (_program == TFUndefinedProgram) {
        ProgramIsUndefinedLog
        return;
    }
    GLint location = glGetUniformLocation(_program, name.c_str());
    if (location >= 0) {
        glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
    }
}

#endif

void TFOPGLProgram::setTexture(std::string name, GLenum textureType, GLuint texture, GLuint textureUnit){
    if (_program == TFUndefinedProgram) {
        ProgramIsUndefinedLog
        return;
    }
    GLint location = glGetUniformLocation(_program, name.c_str());
    if (location >= 0) {
        
        glActiveTexture(GL_TEXTURE0+textureUnit);
        glBindTexture(textureType, texture);
        
        glUniform1i(location, textureUnit);
    }
}

#pragma mark -

bool TFOPGLProgram::use(){
    if (_program == TFUndefinedProgram) {
        ProgramIsUndefinedLog
        return false;
    }
    
    glUseProgram(_program);
    return true;
}
