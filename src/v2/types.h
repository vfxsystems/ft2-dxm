#ifndef TYPES_H_
#define TYPES_H_

#define _CRT_SECURE_NO_DEPRECATE
#define V2TYPES

#include <stdint.h>
#include <stdio.h>

#if !defined(_MSC_VER)
  #ifndef __stdcall
    #define __stdcall
  #endif
  #ifndef __cdecl
    #define __cdecl
  #endif
  // vsprintf_s on MSVC has both 3-arg (using array size template deduction)
  // and 4-arg forms. Use a variadic macro that dispatches on the number of
  // arguments so callers can use either form on non-MSVC compilers.
  #define _FT2_VSP_GET4(a,b,c,d,...) d
  #define _FT2_VSP_3(buf, fmt, ap)         vsnprintf((buf), sizeof(buf), (fmt), (ap))
  #define _FT2_VSP_4(buf, sz, fmt, ap)     vsnprintf((buf), (sz), (fmt), (ap))
  #define _FT2_VSP_DISPATCH(a,b,c,d,FN,...) FN
  #ifndef vsprintf_s
    #define vsprintf_s(...) _FT2_VSP_DISPATCH(__VA_ARGS__, _FT2_VSP_4, _FT2_VSP_3,,)(__VA_ARGS__)
  #endif
#endif

typedef int               sInt;
typedef unsigned int      sUInt;
typedef sInt              sBool;
typedef char              sChar;

typedef signed   char     sS8;
typedef signed   short    sS16;
typedef int32_t           sS32;
#if defined(_MSC_VER)
typedef signed   __int64  sS64;
#else
typedef int64_t           sS64;
#endif

typedef unsigned char     sU8;
typedef unsigned short    sU16;
typedef uint32_t          sU32;
#if defined(_MSC_VER)
typedef unsigned __int64  sU64;
#else
typedef uint64_t          sU64;
#endif

typedef float             sF32;
typedef double            sF64;

#define sTRUE             1
#define sFALSE            0

//
#ifdef _DEBUG
extern void __cdecl printf2(const char *format, ...);
#else
#define printf2
#endif

template<class T> inline T sMin(const T a, const T b) { return (a<b)?a:b;  }
template<class T> inline T sMax(const T a, const T b) { return (a>b)?a:b;  }
template<class T> inline T sClamp(const T x, const T min, const T max) { return sMax(min,sMin(max,x)); }

#endif
