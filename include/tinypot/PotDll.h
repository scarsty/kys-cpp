#pragma once
//#include <Windows.h>
#ifdef _MSC_VER 
#define MYTHAPI _stdcall
#ifdef __cplusplus
#define HBAPI extern "C" __declspec (dllexport)
#else
#define HBAPI __declspec (dllexport)
#endif
#else
#define HBAPI
#define  MYTHAPI
#endif

HBAPI void* MYTHAPI PotCreateFromHandle(void* handle);
HBAPI void* MYTHAPI PotCreateFromWindow(void* handle);
HBAPI int MYTHAPI PotInputVideo(void* pot, char* filename);
HBAPI int MYTHAPI PotSeek(void* pot, int seek);
HBAPI int MYTHAPI PotDestory(void* pot);
