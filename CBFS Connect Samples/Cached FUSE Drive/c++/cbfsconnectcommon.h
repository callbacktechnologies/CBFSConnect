#if !defined _CBFSCONNECT_COMMON_H
#define _CBFSCONNECT_COMMON_H

//disable compiler warnings about functions that was marked with deprecated
#pragma warning(disable : 4996)

#ifdef _UNICODE
#include "../../include/unicode/fuse.h"
#else
#include "../../include/fuse.h"
#endif
/*
inline int64 FileTimeToInt64(FILETIME ft)
{
    return static_cast<int64>(ft.dwHighDateTime) << 32 | ft.dwLowDateTime;
}

inline void Int64ToFileTime(int64 t, LPFILETIME pft)
{
    pft->dwLowDateTime = (DWORD)t;
    pft->dwHighDateTime = t >> 32;
}
*/
#endif //#if !defined _CBFSCONNECT_COMMON_H