#ifndef AVUTIL_INTTYPES_H
#define AVUTIL_INTTYPES_H



//Howard 2013-03-04 ， 解决包含inttypes.h以后出现的Bug
//#include <inttypes.h>
//Howard 2013-03-04 +++begin+++
#if defined(WIN32)  && !defined(__MINGW32__) && !defined(__CYGWIN__)      
#define  CONFIG_WIN32      
#endif      
#if defined(WIN32) && !defined(__MINGW32__)  && !defined(__CYGWIN__) && !defined(EMULATE_INTTYPES)      
#define EMULATE_INTTYPES      
#endif      
#ifndef EMULATE_INTTYPES      
#include  <inttypes.h>     
#else
typedef signed char int8_t;      
typedef  signed short int16_t;      
typedef signed int   int32_t;      
typedef  unsigned char  uint8_t;      
typedef unsigned short uint16_t;      
typedef  unsigned int   uint32_t;      
#ifdef CONFIG_WIN32      
typedef signed  __int64   int64_t;      
typedef unsigned __int64 uint64_t;      
#else /*  other OS */      
typedef signed long long   int64_t;      
typedef  unsigned long long uint64_t;      
#endif /* other OS */      
#endif /*  EMULATE_INTTYPES */


//解决UINT64_C没定义的问题
#ifndef INT64_C
#define INT64_C(c) (c##LL)
#define UINT64_C(c)  (c##ULL)
#endif 

#endif