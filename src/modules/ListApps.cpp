#include "../../include/modules/ListApps.hpp"

std::vector<std::wstring> ListStartMenuApps::GetStartMenuPrograms() {
    std::vector<std::wstring> programs;
    
    // Get Start Menu paths
    PWSTR startMenuPath = NULL;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Programs, 0, NULL, &startMenuPath))) {
        EnumerateShortcuts(startMenuPath, programs);
        CoTaskMemFree(startMenuPath);
    }
    
    // Get Common Start Menu paths
    PWSTR commonStartMenuPath = NULL;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_CommonPrograms, 0, NULL, &commonStartMenuPath))) {
        EnumerateShortcuts(commonStartMenuPath, programs);
        CoTaskMemFree(commonStartMenuPath);
    }
    
    return programs;
}
std::string ListStartMenuApps::GetAllApps() {
    std::vector<std::wstring> apps = GetStartMenuPrograms();
    std::stringstream ss;
    for (size_t i = 0; i < apps.size(); ++i) {
        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        ss << (i + 1) << ". " << converter.to_bytes(apps[i]) << "<br>";
    }
    return ss.str();
}
void ListStartMenuApps::EnumerateShortcuts(const std::wstring& folderPath, std::vector<std::wstring>& programs) {
    std::wstring searchPath = folderPath + L"\\*.lnk";
    
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
    
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                std::wstring fileName = findData.cFileName;
                // Remove .lnk extension
                if (fileName.size() > 4 && fileName.substr(fileName.size() - 4) == L".lnk") {
                    programs.push_back(fileName.substr(0, fileName.size() - 4));
                }
            }
        } while (FindNextFileW(hFind, &findData));
        
        FindClose(hFind);
    }
}