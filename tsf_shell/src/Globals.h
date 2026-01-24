#pragma once

#include <windows.h>

extern HINSTANCE g_hInstance;
extern LONG g_dllRefCount;

inline void DllAddRef() { InterlockedIncrement(&g_dllRefCount); }
inline void DllRelease() { InterlockedDecrement(&g_dllRefCount); }

