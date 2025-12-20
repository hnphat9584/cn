#pragma once
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

// Định nghĩa GUID cho H.264 nếu chưa có
#ifndef MFVideoFormat_H264
DEFINE_GUID(MFVideoFormat_H264, 0x34363248, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
#endif

class ScreenRecorder {
public:
    static void FlipImageVertically(BYTE* data, int width, int height, int bytesPerPixel);
    static void record(int RECORD_SECONDS = 10);
};

// g++ record.cpp -o record.exe -lole32 -lmf -lmfplat -lmfreadwrite -lmfuuid