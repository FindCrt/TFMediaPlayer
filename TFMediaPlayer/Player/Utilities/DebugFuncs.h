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
if (retval != 0) {\
printf("%s\n",statement);\
}\

#define TFCheckRetvalAndReturn(statement)\
if (retval != 0) {\
printf("%s\n",statement);\
return;\
}\

#define TFCheckRetvalAndGotoFail(statement)\
if (retval != 0) {\
printf("%s\n",statement);\
goto Fail;\
}\


#endif /* DebugFuncs_h */
