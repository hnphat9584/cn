#include "../../include/modules/CameraController.hpp"

CameraController::~CameraController() {
    Cleanup();
}

bool CameraController::Initialize() {
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) return false;

    hr = MFStartup(MF_VERSION);
    return SUCCEEDED(hr);
}

void CameraController::Cleanup() {
    if (m_pReader) m_pReader->Release();
    if (m_pSource) m_pSource->Release();
    if (m_pWriter) m_pWriter->Release();
    MFShutdown();
    CoUninitialize();
}

// Liệt kê các camera
std::vector<std::wstring> CameraController::EnumCameras() {
    std::vector<std::wstring> cameras;
    
    IMFAttributes* pAttributes = nullptr;
    IMFActivate** ppDevices = nullptr;
    UINT32 count = 0;

    // Tạo attributes để lấy video capture devices
    MFCreateAttributes(&pAttributes, 1);
    pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, 
                        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);

    // Liệt kê devices
    MFEnumDeviceSources(pAttributes, &ppDevices, &count);

    for (UINT32 i = 0; i < count; i++) {
        wchar_t* name = nullptr;
        UINT32 nameLength = 0;
        
        ppDevices[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, 
                                    &name, &nameLength);
        cameras.push_back(name);
        CoTaskMemFree(name);
        ppDevices[i]->Release();
    }

    CoTaskMemFree(ppDevices);
    pAttributes->Release();
    
    return cameras;
}

bool CameraController::SaveBufferAsImage(IMFMediaBuffer* pBuffer, const std::wstring& outputFile)
{
    if (!pBuffer)
        return false;

    HRESULT hr = S_OK;

    // Lock the buffer
    BYTE* pData = nullptr;
    DWORD maxLen = 0, curLen = 0;

    hr = pBuffer->Lock(&pData, &maxLen, &curLen);
    if (FAILED(hr))
        return false;

    // IMPORTANT ASSUMPTION:
    // The buffer contains raw RGB32 (ARGB) pixel data.
    // If your camera outputs NV12/YUY2/etc you must convert beforehand!
    UINT width = 640;     // <-- you must set real width
    UINT height = 480;    // <-- you must set real height

    // Create WIC Factory
    Microsoft::WRL::ComPtr<IWICImagingFactory> pFactory;
    hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&pFactory)
    );

    if (FAILED(hr)) {
        pBuffer->Unlock();
        return false;
    }

    // Create a WIC Bitmap from memory
    Microsoft::WRL::ComPtr<IWICBitmap> pBitmap;
    hr = pFactory->CreateBitmapFromMemory(
        width,
        height,
        GUID_WICPixelFormat32bppBGRA,  // matches RGB32
        width * 4,
        curLen,
        pData,
        &pBitmap
    );

    // Done using raw buffer
    pBuffer->Unlock();

    if (FAILED(hr))
        return false;

    // Create a stream for output file
    Microsoft::WRL::ComPtr<IWICStream> pStream;
    hr = pFactory->CreateStream(&pStream);
    if (FAILED(hr))
        return false;

    hr = pStream->InitializeFromFilename(outputFile.c_str(), GENERIC_WRITE);
    if (FAILED(hr))
        return false;

    // Create encoder (PNG)
    Microsoft::WRL::ComPtr<IWICBitmapEncoder> pEncoder;
    hr = pFactory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &pEncoder);
    if (FAILED(hr))
        return false;

    hr = pEncoder->Initialize(pStream.Get(), WICBitmapEncoderNoCache);
    if (FAILED(hr))
        return false;

    // Create frame
    Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> pFrame;
    Microsoft::WRL::ComPtr<IPropertyBag2> pProps;
    hr = pEncoder->CreateNewFrame(&pFrame, &pProps);
    if (FAILED(hr))
        return false;

    hr = pFrame->Initialize(pProps.Get());
    if (FAILED(hr))
        return false;

    // image dimensions
    hr = pFrame->SetSize(width, height);
    if (FAILED(hr))
        return false;

    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
    hr = pFrame->SetPixelFormat(&format);
    if (FAILED(hr))
        return false;

    // write pixels
    hr = pFrame->WriteSource(pBitmap.Get(), nullptr);
    if (FAILED(hr))
        return false;

    // Commit writer
    pFrame->Commit();
    pEncoder->Commit();

    return true;
}

// Chụp ảnh
bool CameraController::CapturePhoto(const std::wstring& outputFile) {
    IMFMediaSource* pSource = nullptr;
    IMFSourceReader* pReader = nullptr;
    IMFMediaType* pType = nullptr;
    IMFSample* pSample = nullptr;
    IMFMediaBuffer* pBuffer = nullptr;

    // Chọn camera đầu tiên
    auto cameras = EnumCameras();
    if (cameras.empty()) return false;

    // Tạo media source từ camera
    IMFAttributes* pAttributes = nullptr;
    MFCreateAttributes(&pAttributes, 1);
    pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, 
                        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);

    IMFActivate** ppDevices = nullptr;
    UINT32 count = 0;
    MFEnumDeviceSources(pAttributes, &ppDevices, &count);
    
    if (count > 0) {
        ppDevices[0]->ActivateObject(IID_PPV_ARGS(&pSource));
        
        // Tạo source reader
        MFCreateSourceReaderFromMediaSource(pSource, nullptr, &pReader);
        
        // Chọn media type (RGB32)
        MFCreateMediaType(&pType);
        pType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        pType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
        pReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, pType);
        
        // Đọc frame
        DWORD streamIndex, flags;
        LONGLONG timestamp;
        pReader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, 
                        &streamIndex, &flags, &timestamp, &pSample);
        
        if (pSample) {
            // Lấy buffer và lưu ảnh
            pSample->ConvertToContiguousBuffer(&pBuffer);
            
            // Ở đây bạn có thể encode buffer thành JPEG/PNG và lưu file
            SaveBufferAsImage(pBuffer, outputFile);
        }
    }

    // Cleanup
    if (pBuffer) pBuffer->Release();
    if (pSample) pSample->Release();
    if (pType) pType->Release();
    if (pReader) pReader->Release();
    if (pSource) pSource->Release();
    
    return true;
}

// Ghi video
bool CameraController::StartRecording(const std::wstring& outputFile) {
    IMFMediaSource* pSource = nullptr;
    IMFSourceReader* pReader = nullptr;
    
    // Tương tự như chụp ảnh, nhưng setup cho video
    auto cameras = EnumCameras();
    if (cameras.empty()) return false;

    // Tạo sink writer để ghi video
    IMFAttributes* pAttributes = nullptr;
    MFCreateAttributes(&pAttributes, 1);
    pAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);

    // Tạo MP4 sink writer
    MFCreateSinkWriterFromURL(outputFile.c_str(), nullptr, pAttributes, &m_pWriter);
    
    // Cấu hình video stream
    IMFMediaType* pType = nullptr;
    MFCreateMediaType(&pType);
    pType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    pType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    pType->SetUINT32(MF_MT_AVG_BITRATE, 800000); // 800 kbps
    pType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    
    // Thêm stream vào writer
    DWORD streamIndex;
    m_pWriter->AddStream(pType, &streamIndex);
    m_pWriter->SetInputMediaType(streamIndex, pType, nullptr);
    
    // Bắt đầu ghi
    m_pWriter->BeginWriting();
    
    return true;
}

// lưu file
void CameraController::RecordFrame() {
    if (!m_pReader || !m_pWriter) return;
    
    IMFSample* pSample = nullptr;
    DWORD streamIndex, flags;
    LONGLONG timestamp;
    
    // Đọc frame từ camera
    m_pReader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, 
                        &streamIndex, &flags, &timestamp, &pSample);
    
    if (pSample) {
        // Ghi frame vào video
        m_pWriter->WriteSample(0, pSample);
        pSample->Release();
    }
}

void CameraController::StopRecording() {
    if (m_pWriter) {
        m_pWriter->Finalize();
        m_pWriter->Release();
        m_pWriter = nullptr;
    }
}

// Điều chỉnh cài đặt camera
bool CameraController::SetCameraSettings(int brightness, int contrast, int exposure) {
    IAMCameraControl* pCameraControl = nullptr;
    IAMVideoProcAmp* pVideoProcAmp = nullptr;
    
    // Lấy interfaces để điều khiển camera
    m_pSource->QueryInterface(IID_PPV_ARGS(&pCameraControl));
    m_pSource->QueryInterface(IID_PPV_ARGS(&pVideoProcAmp));
    
    if (pVideoProcAmp) {
        // Điều chỉnh brightness
        pVideoProcAmp->Set(VideoProcAmp_Brightness, brightness, VideoProcAmp_Flags_Manual);
        
        // Điều chỉnh contrast
        pVideoProcAmp->Set(VideoProcAmp_Contrast, contrast, VideoProcAmp_Flags_Manual);
    }
    
    if (pCameraControl) {
        // Điều chỉnh exposure
        pCameraControl->Set(CameraControl_Exposure, exposure, CameraControl_Flags_Manual);
    }
    
    if (pCameraControl) pCameraControl->Release();
    if (pVideoProcAmp) pVideoProcAmp->Release();
    
    return true;
}