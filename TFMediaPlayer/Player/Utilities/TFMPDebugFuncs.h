//
//  TFMPDebugFuncs.h
//  TFMediaPlayer
//
//  Created by shiwei on 17/12/28.
//  Copyright © 2017年 shiwei. All rights reserved.
//

#ifndef TFMPDebugFuncs_h
#define TFMPDebugFuncs_h

/** log */

#define TFMPLOG 1

#if DEBUG & TFMPLOG
#define TFMPDLog(format,...) NSLog(format, ##__VA_ARGS__)
#else
#define TFMPDLog(format,...)
#endif

/** function status check */

#if DEBUG & TFMPLOG

#define TFCheckRetval(statement)\
if (retval < 0) {\
char errStr[64];\
av_strerror(retval, errStr, 64);\
printf("%s error(%s)\n",statement,errStr);\
}\

#define TFCheckRetvalAndReturn(statement)\
if (retval < 0) {\
char errStr[64];\
av_strerror(retval, errStr, 64);\
printf("%s error(%s)\n",statement,errStr);\
return;\
}\

#define TFCheckRetvalAndGotoFail(statement)\
if (retval < 0) {\
char errStr[64];\
av_strerror(retval, errStr, 64);\
printf("%s error(%s)\n",statement,errStr);\
goto fail;\
}\




#define TFCheckStatus(status, log)    if(status != 0) {\
int bigEndian = CFSwapInt32HostToBig(status);\
char *statusTex = (char*)&bigEndian;\
NSLog(@"%@ error: %s",log,statusTex); return;\
}

#define TFCheckStatusReturnStatus(status, log)    if(status != 0) {\
int bigEndian = CFSwapInt32HostToBig(status);\
char *statusTex = (char*)&bigEndian;\
NSLog(@"%@ error: %s",log,statusTex); return status;\
}

#define TFCheckStatusUnReturn(status, log)    if(status != 0) {\
uint64_t bigEndian = CFSwapInt32HostToBig(status);\
char *statusTex = (char*)&bigEndian;\
NSLog(@"%@ error: %s",log,statusTex);\
}

#define TFCheckStatusGoToFail(status, log)    if(status != 0) {\
int bigEndian = CFSwapInt32HostToBig(status);\
char *statusTex = (char*)&bigEndian;\
NSLog(@"%@ error: %s",log,statusTex); goto fail;\
}

#define TFCheckError(error, log)    if(error) {\
NSLog(@"%@ error:\n{%@}",log,error); return;\
}

/** check buffer */

#define TFMPPrintBuffer(buffer, start, length)\
int *checkP = (int*)buffer;\
for(int i = 0; i<length;i++){\
    printf("%x |",*checkP);\
    checkP++;\
}\
printf("\n-------------\n");

#endif

#endif /* TFMPDebugFuncs_h */
