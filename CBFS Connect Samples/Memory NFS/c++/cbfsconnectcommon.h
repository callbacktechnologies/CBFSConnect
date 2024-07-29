#if !defined _CBFSCONNECT_COMMON_H
#define _CBFSCONNECT_COMMON_H

//disable compiler warnings about functions that was marked with deprecated
#pragma warning(disable : 4996)

#ifdef _UNICODE
#include "../../include/unicode/nfs.h"
#else
#include "../../include/nfs.h"
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

#ifdef UNIX
inline int64 UnixTimeToFileTime(time_t unixTime, long long nanoSeconds) 
{    
    return (unixTime + 11644473600) * 10000000 + nanoSeconds / 100;
}
#endif

#endif //#if !defined _CBFSCONNECT_COMMON_H