#pragma once
#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <locale>
#include <codecvt>

class WindowCollector {
private:
    std::vector<std::string> windowList;
    std::stringstream windowData;

public:
    // Callback function cho EnumWindows
    static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);
    BOOL CollectWindowInfo(HWND hwnd);
    void CollectAllWindows();

    std::vector<std::string> GetWindowList() const;
    std::string GetWindowsAsString() const;

    std::string GetFormattedString() const;
};