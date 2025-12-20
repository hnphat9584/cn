#include "../../include/modules/ScreenRecorder.hpp"


void ScreenRecorder::FlipImageVertically(BYTE* data, int width, int height, int bytesPerPixel) {
    int rowSize = width * bytesPerPixel;
    std::vector<BYTE> tempRow(rowSize);
    
    for (int y = 0; y < height / 2; y++) {
        BYTE* topRow = data + y * rowSize;
        BYTE* bottomRow = data + (height - 1 - y) * rowSize;
        
        memcpy(tempRow.data(), topRow, rowSize);
        memcpy(topRow, bottomRow, rowSize);
        memcpy(bottomRow, tempRow.data(), rowSize);
    }
}
void ScreenRecorder::record(int RECORD_SECONDS) {
    const UINT32 WIDTH = 1280;
    const UINT32 HEIGHT = 720;
    const UINT32 FPS = 30;
    const DWORD BUFFER_SIZE = WIDTH * HEIGHT * 4; // RGB32
    const LONGLONG FRAME_DURATION = 10'000'000 / FPS; // 100-nanosecond units
    
    // 1. KHỞI TẠO
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    
    hr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
    
    // 2. TẠO TÊN FILE
    wchar_t filename[] = L"Video.mp4";
    
    // 3. TẠO SINK WRITER
    IMFSinkWriter* pWriter = nullptr;
    
    IMFAttributes* pWriterAttributes = nullptr;
    MFCreateAttributes(&pWriterAttributes, 2);
    pWriterAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
    pWriterAttributes->SetUINT32(MF_SINK_WRITER_DISABLE_THROTTLING, TRUE);
    
    hr = MFCreateSinkWriterFromURL(filename, nullptr, pWriterAttributes, &pWriter);
    pWriterAttributes->Release();
    
    // 4. CẤU HÌNH OUTPUT (H.264)
    IMFMediaType* pVideoOutType = nullptr;
    hr = MFCreateMediaType(&pVideoOutType);
    
    pVideoOutType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    pVideoOutType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    pVideoOutType->SetUINT32(MF_MT_AVG_BITRATE, 4000000); // 4 Mbps cho 1280x720
    pVideoOutType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    MFSetAttributeSize(pVideoOutType, MF_MT_FRAME_SIZE, WIDTH, HEIGHT);
    MFSetAttributeRatio(pVideoOutType, MF_MT_FRAME_RATE, FPS, 1);
    MFSetAttributeRatio(pVideoOutType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    
    DWORD videoStreamIndex = 0;
    hr = pWriter->AddStream(pVideoOutType, &videoStreamIndex);
    pVideoOutType->Release();
    
    // 5. CẤU HÌNH INPUT (RGB32)
    IMFMediaType* pVideoInType = nullptr;
    hr = MFCreateMediaType(&pVideoInType);
    
    pVideoInType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    pVideoInType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    pVideoInType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    MFSetAttributeSize(pVideoInType, MF_MT_FRAME_SIZE, WIDTH, HEIGHT);
    MFSetAttributeRatio(pVideoInType, MF_MT_FRAME_RATE, FPS, 1);
    MFSetAttributeRatio(pVideoInType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    
    hr = pWriter->SetInputMediaType(videoStreamIndex, pVideoInType, nullptr);
    pVideoInType->Release();
    
    // 6. TÌM VÀ CẤU HÌNH CAMERA
    IMFMediaSource* pVideoSource = nullptr;
    IMFActivate** ppVideoDevices = nullptr;
    UINT32 videoDeviceCount = 0;
    bool useCamera = false;
    
    IMFAttributes* pVideoConfig = nullptr;
    hr = MFCreateAttributes(&pVideoConfig, 1);
    if (SUCCEEDED(hr)) {
        hr = pVideoConfig->SetGUID(
            MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
            MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID
        );
        
        if (SUCCEEDED(hr)) {
            hr = MFEnumDeviceSources(pVideoConfig, &ppVideoDevices, &videoDeviceCount);
        }
    }
    
    IMFSourceReader* pReader = nullptr;
    if (SUCCEEDED(hr) && videoDeviceCount > 0) {
        hr = ppVideoDevices[0]->ActivateObject(IID_PPV_ARGS(&pVideoSource));
        if (SUCCEEDED(hr)) {
            IMFAttributes* pReaderAttributes = nullptr;
            hr = MFCreateAttributes(&pReaderAttributes, 1);
            if (SUCCEEDED(hr)) {
                pReaderAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
                
                hr = MFCreateSourceReaderFromMediaSource(pVideoSource, pReaderAttributes, &pReader);
                pReaderAttributes->Release();
                
                if (SUCCEEDED(hr)) {
                    IMFMediaType* pReaderType = nullptr;
                    hr = MFCreateMediaType(&pReaderType);
                    if (SUCCEEDED(hr)) {
                        pReaderType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
                        pReaderType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
                        pReaderType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
                        MFSetAttributeSize(pReaderType, MF_MT_FRAME_SIZE, WIDTH, HEIGHT);
                        MFSetAttributeRatio(pReaderType, MF_MT_FRAME_RATE, FPS, 1);
                        
                        hr = pReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, pReaderType);
                        pReaderType->Release();
                        
                        if (SUCCEEDED(hr)) {
                            useCamera = true;
                        }
                    }
                }
            }
        }
    }
    
    if (!useCamera) {
        std::cout << "No camera available or camera setup failed. Creating test video...\n";
    }
    
    // 7. BẮT ĐẦU GHI
    hr = pWriter->BeginWriting();
    
    // 8. QUAY VIDEO - SỬ DỤNG ĐỒNG HỒ THỜI GIAN THỰC
    auto startTime = std::chrono::steady_clock::now();
    int frameCount = 0;
    
    if (useCamera && pReader) {
        // QUAY TỪ CAMERA - SỬ DỤNG TIMESTAMP THỰC TỪ CAMERA
        while (true) {
            auto currentTime = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();
            
            if (elapsed >= RECORD_SECONDS) break;
            
            DWORD streamFlags = 0;
            LONGLONG timestamp = 0;
            IMFSample* pSample = nullptr;
            
            hr = pReader->ReadSample(
                MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                0, nullptr, &streamFlags, &timestamp, &pSample
            );
            
            if (FAILED(hr) || (streamFlags & MF_SOURCE_READERF_ENDOFSTREAM)) {
                break;
            }
            
            if (pSample) {
                // Lật ảnh nếu cần
                IMFMediaBuffer* pBuffer = nullptr;
                hr = pSample->ConvertToContiguousBuffer(&pBuffer);
                
                if (SUCCEEDED(hr)) {
                    BYTE* pData = nullptr;
                    DWORD maxLength = 0, currentLength = 0;
                    hr = pBuffer->Lock(&pData, &maxLength, &currentLength);
                    
                    if (SUCCEEDED(hr) && currentLength >= BUFFER_SIZE) {
                        FlipImageVertically(pData, WIDTH, HEIGHT, 4);
                        pBuffer->Unlock();
                        
                        // Sử dụng timestamp thực từ camera hoặc tính từ startTime
                        auto frameTime = std::chrono::steady_clock::now();
                        auto timeFromStart = std::chrono::duration_cast<std::chrono::nanoseconds>(frameTime - startTime);
                        LONGLONG sampleTime = timeFromStart.count() / 100; // Chuyển sang 100-nanosecond units
                        
                        pSample->SetSampleTime(sampleTime);
                        pSample->SetSampleDuration(FRAME_DURATION);
                        
                        hr = pWriter->WriteSample(videoStreamIndex, pSample);
                        frameCount++;
                    }
                    
                    pBuffer->Release();
                }
                
                pSample->Release();
            }
        }
    } else {
        // TẠO VIDEO TEST - TÍNH TOÁN CHÍNH XÁC THỜI GIAN
        const int DESIRED_FRAMES = RECORD_SECONDS * FPS;
        auto frameStartTime = startTime;
        
        for (int i = 0; i < DESIRED_FRAMES; i++) {
            // Tính thời gian cho frame này
            auto targetTime = startTime + std::chrono::milliseconds(i * 1000 / FPS);
            
            // Chờ đến đúng thời điểm của frame
            auto now = std::chrono::steady_clock::now();
            if (now < targetTime) {
                std::this_thread::sleep_until(targetTime);
            }
            
            // Tạo sample
            IMFSample* pSample = nullptr;
            hr = MFCreateSample(&pSample);
            
            // Tạo buffer
            IMFMediaBuffer* pBuffer = nullptr;
            hr = MFCreateMemoryBuffer(BUFFER_SIZE, &pBuffer);
            
            BYTE* pData = nullptr;
            DWORD maxLength = 0;
            hr = pBuffer->Lock(&pData, &maxLength, nullptr);
            
            // Tạo frame với hiệu ứng xoay màu
            for (UINT32 y = 0; y < HEIGHT; y++) {
                for (UINT32 x = 0; x < WIDTH; x++) {
                    UINT32 offset = (y * WIDTH + x) * 4;
                    
                    // Tạo gradient xoay
                    int hue = (i * 3 + x / 4 + y / 4) % 360;
                    
                    // Chuyển HSV sang RGB
                    float h = hue / 60.0f;
                    int sector = static_cast<int>(h);
                    float fraction = h - sector;
                    
                    BYTE p = 0;
                    BYTE q = static_cast<BYTE>(255 * (1.0f - fraction));
                    BYTE t = static_cast<BYTE>(255 * fraction);
                    
                    switch (sector % 6) {
                        case 0: pData[offset + 2] = 255; pData[offset + 1] = t; pData[offset] = p; break;
                        case 1: pData[offset + 2] = q; pData[offset + 1] = 255; pData[offset] = p; break;
                        case 2: pData[offset + 2] = p; pData[offset + 1] = 255; pData[offset] = t; break;
                        case 3: pData[offset + 2] = p; pData[offset + 1] = q; pData[offset] = 255; break;
                        case 4: pData[offset + 2] = t; pData[offset + 1] = p; pData[offset] = 255; break;
                        case 5: pData[offset + 2] = 255; pData[offset + 1] = p; pData[offset] = q; break;
                    }
                    
                    pData[offset + 3] = 255; // Alpha
                }
            }
            
            pBuffer->Unlock();
            pBuffer->SetCurrentLength(BUFFER_SIZE);
            
            pSample->AddBuffer(pBuffer);
            pBuffer->Release();
            
            // Đặt timestamp chính xác
            LONGLONG sampleTime = i * FRAME_DURATION;
            pSample->SetSampleTime(sampleTime);
            pSample->SetSampleDuration(FRAME_DURATION);
            
            pWriter->WriteSample(videoStreamIndex, pSample);
            pSample->Release();
            frameCount++;
        }
    }
    
    auto endTime = std::chrono::steady_clock::now();
    auto totalDuration = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime).count();
    
    hr = pWriter->Finalize();
    
    // 10. DỌN DẸP
    if (pWriter) pWriter->Release();
    if (pReader) pReader->Release();
    if (pVideoSource) {
        pVideoSource->Shutdown();
        pVideoSource->Release();
    }
    
    if (ppVideoDevices) {
        for (UINT32 i = 0; i < videoDeviceCount; i++) {
            if (ppVideoDevices[i]) ppVideoDevices[i]->Release();
        }
        CoTaskMemFree(ppVideoDevices);
    }
    
    if (pVideoConfig) pVideoConfig->Release();
    
    MFShutdown();
    CoUninitialize();
}