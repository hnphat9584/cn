#pragma once

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <iostream>
#include <vector>
#include <strmif.h>
#include <wincodec.h>     // WIC for saving images
#include <wrl/client.h>   // Microsoft::WRL::ComPtr

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

class CameraController {
private:
    IMFMediaSource* m_pSource = nullptr;
    IMFSourceReader* m_pReader = nullptr;
    IMFSinkWriter* m_pWriter = nullptr;

public:
    ~CameraController();
    bool Initialize();
    void Cleanup();
    // Liệt kê các camera
    std::vector<std::wstring> EnumCameras();
    // Chụp ảnh
    bool CapturePhoto(const std::wstring& outputFile);
    // Ghi video
    bool StartRecording(const std::wstring& outputFile);
    void StopRecording();
    // lưu file
    void RecordFrame();
    bool SaveBufferAsImage(IMFMediaBuffer* pBuffer, const std::wstring& outputFile);
    // Điều chỉnh cài đặt camera
    bool SetCameraSettings(int brightness, int contrast, int exposure);
};