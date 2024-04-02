#if !defined _CBFSCONNECT_COMMON_H
#define _CBFSCONNECT_COMMON_H

//disable compiler warnings about functions that was marked with deprecated
#pragma warning(disable : 4996)

#ifdef _UNICODE
#include "../../include/unicode/cbfs.h"
#else
#include "../../include/cbfs.h"
#endif

inline __int64 FileTimeToInt64(FILETIME ft)
{
	return static_cast<__int64>(ft.dwHighDateTime) << 32 | ft.dwLowDateTime;
}

inline void Int64ToFileTime(__int64 t, LPFILETIME pft)
{
	pft->dwLowDateTime = (DWORD)t;
	pft->dwHighDateTime = t >> 32;
}

#endif //#if !defined _CBFSCONNECT_COMMON_H