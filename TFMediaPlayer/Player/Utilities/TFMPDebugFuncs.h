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

#if DEBUG & TFMPLOG
#define TFMPDLOG_C(format,...) printf(format, ##__VA_ARGS__)
#else
#define TFMPDLOG_C(format,...)
#endif

/** function status check */

#if DEBUG & TFMPLOG

#define TFCheckRetval(statement)\
if (retval < 0) {\
char errStr[64];\
av_strerror(retval, errStr, 64);\
printf("%s error(%s)\n",statement,errStr);\
}\

#define TFCheckRetvalAndReturnFalse(statement)\
if (retval < 0) {\
char errStr[64];\
av_strerror(retval, errStr, 64);\
printf("%s error(%s)\n",statement,errStr);\
return false;\
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

#define TFMPPrintBuffer_S16(buffer, start, length)\
signed short *checkP = ((signed short*)buffer)+start;\
for(int i = 0; i<length;i++){\
    printf("%d ",*checkP);\
    checkP++;\
}\
printf("\n-------------\n");

#define TFMPPrintBuffer(buffer, start, length)\
uint8_t *checkP = ((uint8_t*)buffer)+start;\
for(int i = 0; i<length;i++){\
printf("%x ",*checkP);\
checkP++;\
}\
printf("\n-------------\n");

#endif

#endif /* TFMPDebugFuncs_h */
