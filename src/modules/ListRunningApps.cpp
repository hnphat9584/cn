#include "../../include/modules/ListRunningApps.hpp"

BOOL CALLBACK WindowCollector::EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    WindowCollector* collector = reinterpret_cast<WindowCollector*>(lParam);
    return collector->CollectWindowInfo(hwnd);
}

BOOL WindowCollector::CollectWindowInfo(HWND hwnd) {
    if (IsWindowVisible(hwnd)) {
        wchar_t windowTitle[10000];
        GetWindowTextW(hwnd, windowTitle, sizeof(windowTitle)/sizeof(wchar_t)); // Sử dụng W version
        
        if (wcslen(windowTitle) > 0) {
            // Chuyển từ wstring (UTF-16) sang string (UTF-8)
            std::wstring wideTitle(windowTitle);
            std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
            std::string utf8Title = converter.to_bytes(wideTitle);
            
            windowList.push_back(utf8Title);
            windowData << utf8Title << "\n";
        }
    }
    return TRUE;
}

void WindowCollector::CollectAllWindows() {
    windowList.clear();
    windowData.str("");
    windowData.clear();
    
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(this));
}

std::vector<std::string> WindowCollector::GetWindowList() const {
    return windowList;
}

std::string WindowCollector::GetWindowsAsString() const {
    return windowData.str();
}

std::string WindowCollector::GetFormattedString() const {
    std::stringstream ss;
    for (size_t i = 0; i < windowList.size(); ++i) {
        ss << (i + 1) << ". " << windowList[i] << "<br>";
    }
    return ss.str();
}