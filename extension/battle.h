#pragma once
#ifdef _WIN32
#define _STDCALL __stdcall
#define _DLLEXPORT __declspec(dllexport)
#else
#define _STDCALL
#define _DLLEXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif
    _DLLEXPORT void* _STDCALL battlescene();
#ifdef __cplusplus
}
#endif
