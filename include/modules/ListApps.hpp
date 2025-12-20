#pragma once
#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <shlobj.h>
#include <objbase.h>
#include <shobjidl.h>
#include <knownfolders.h>
#include <locale>
#include <codecvt>
#include <sstream>

class ListStartMenuApps {
public:
    static std::vector<std::wstring> GetStartMenuPrograms();
    std::string GetAllApps();
private:

    static void EnumerateShortcuts(const std::wstring& folderPath, std::vector<std::wstring>& programs);
};