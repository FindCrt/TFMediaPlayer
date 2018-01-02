//
//  DebugFuncs.h
//  TFMediaPlayer
//
//  Created by shiwei on 17/12/28.
//  Copyright © 2017年 shiwei. All rights reserved.
//

#ifndef DebugFuncs_h
#define DebugFuncs_h

#define TFCheckRetval(statement)\
if (retval < 0) {\
char errStr[64];\
av_strerror(retval, errStr, 64);\
printf("%s error(%d)\n",statement,retval);\
}\

#define TFCheckRetvalAndReturn(statement)\
if (retval < 0) {\
char errStr[64];\
av_strerror(retval, errStr, 64);\
printf("%s error(%d)\n",statement,retval);\
return;\
}\

#define TFCheckRetvalAndGotoFail(statement)\
if (retval < 0) {\
char errStr[64];\
av_strerror(retval, errStr, 64);\
printf("%s error(%s)\n",statement,errStr);\
goto fail;\
}\


#endif /* DebugFuncs_h */
