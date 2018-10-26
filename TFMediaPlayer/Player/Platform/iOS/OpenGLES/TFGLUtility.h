//
//  TFGLUtility.h
//  TFMediaPlayer
//
//  Created by shiwei on 18/1/25.
//  Copyright © 2018年 shiwei. All rights reserved.
//

#ifndef TFGLUtility_h
#define TFGLUtility_h

#define KAdoptES2 0

#define TFGLShaderSource_sharp(s) "#"#s
#define TFGLShaderSource(s) #s

//切换context执行一段代码，再切回去
#define TFGLSwitchContextToDo(ctx, code)\
EAGLContext *preContext = [EAGLContext currentContext];\
[EAGLContext setCurrentContext:ctx];\
code\
[EAGLContext setCurrentContext:preContext];\

#define CGSizeMultiply(size, factor) CGSizeMake(size.width*factor, size.height*factor)

#endif /* TFGLUtility_h */
