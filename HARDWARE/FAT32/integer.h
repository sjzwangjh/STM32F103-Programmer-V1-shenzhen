/*
 * FatFs整数类型定义头文件 - 跨平台整型类型映射（BYTE/WORD/DWORD等）
 */

/*-------------------------------------------*/
/* Integer type definitions for FatFs module */
/*-------------------------------------------*/

#ifndef _INTEGER_H
#define _INTEGER_H

#ifdef _WIN32         /* FatFs development platform */
#include <windows.h>
#include <tchar.h>
typedef unsigned __int64 QWORD;
#else                 /* Embedded platform */
#include "sys.h"

/* These types MUST be 16-bit or 32-bit */
typedef int             INT;
typedef unsigned int    UINT;

/* These types MUST be 8-bit */
typedef unsigned char   BYTE;

/* These types MUST be 16-bit */
typedef short           SHORT;
typedef unsigned short  WORD;
typedef unsigned short  WCHAR;

/* These types MUST be 32-bit */
typedef long           LONG;
typedef unsigned long  DWORD;

/* This type MUST be 64-bit (Remove this if C89 compatible) */
typedef unsigned long long QWORD;

#endif

#endif


