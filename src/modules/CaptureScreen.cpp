#include "../../include/modules/CaptureScreen.hpp"

void CaptureScreen::run() {
    HDC hScreenDC = GetDC(NULL);
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
    
    int width = GetSystemMetrics(SM_CXSCREEN);
    int height = GetSystemMetrics(SM_CYSCREEN);
    
    // 2. Tạo bitmap
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemoryDC, hBitmap);
    
    // 3. Chụp màn hình
    BitBlt(hMemoryDC, 0, 0, width, height, hScreenDC, 0, 0, SRCCOPY);
    
    // 4. Lưu file BMP đơn giản nhất
    BITMAPFILEHEADER bmfHeader = {0};
    BITMAPINFOHEADER bi = {0};
    
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = width;
    bi.biHeight = height;
    bi.biPlanes = 1;
    bi.biBitCount = 24;  // Dùng 24-bit RGB thay vì lấy từ màn hình
    bi.biCompression = BI_RGB;
    
    DWORD dwBmpSize = ((width * bi.biBitCount + 31) / 32) * 4 * height;
    
    // 5. Lấy dữ liệu pixel
    BYTE* bits = (BYTE*)malloc(dwBmpSize);
    GetDIBits(hMemoryDC, hBitmap, 0, height, bits, (BITMAPINFO*)&bi, DIB_RGB_COLORS);
    
    // 6. Tạo file header
    bmfHeader.bfType = 0x4D42;  // 'BM'
    bmfHeader.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + dwBmpSize;
    bmfHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    
    // 7. Ghi file
    HANDLE hFile = CreateFileA("screen.bmp", GENERIC_WRITE, 0, NULL, 
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD dwWritten;
        WriteFile(hFile, &bmfHeader, sizeof(BITMAPFILEHEADER), &dwWritten, NULL);
        WriteFile(hFile, &bi, sizeof(BITMAPINFOHEADER), &dwWritten, NULL);
        WriteFile(hFile, bits, dwBmpSize, &dwWritten, NULL);
        CloseHandle(hFile);
    }
    
    // 8. Dọn dẹp
    free(bits);
    SelectObject(hMemoryDC, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hMemoryDC);
    ReleaseDC(NULL, hScreenDC);
}