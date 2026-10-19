#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <string>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    wchar_t currentDir[MAX_PATH];
    GetModuleFileNameW(NULL, currentDir, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(currentDir, L'\\');
    if (lastSlash) *lastSlash = L'\0';

    std::wstring appBinDir = std::wstring(currentDir) + L"\\app\\build\\bin";
    std::wstring appExe = appBinDir + L"\\Catchim.exe";

    // Forward command line parameters if any
    LPWSTR cmdLine = GetCommandLineW();
    std::wstring params = L"";
    if (cmdLine) {
        std::wstring originalCmd(cmdLine);
        size_t firstQuote = originalCmd.find(L"\"");
        if (firstQuote != std::wstring::npos) {
            size_t secondQuote = originalCmd.find(L"\"", firstQuote + 1);
            if (secondQuote != std::wstring::npos && secondQuote + 1 < originalCmd.size()) {
                params = originalCmd.substr(secondQuote + 1);
            }
        }
    }

    HINSTANCE res = ShellExecuteW(
        NULL,
        L"open",
        appExe.c_str(),
        params.empty() ? NULL : params.c_str(),
        appBinDir.c_str(),
        SW_SHOWNORMAL
    );

    if ((INT_PTR)res > 32) {
        return 0;
    }

    // Fallback: Show informative error dialog
    MessageBoxW(
        NULL,
        (L"Khong tim thay file thuc thi Catchim tai:\n" + appExe + L"\nVui long kiem tra thu muc app/build/bin!").c_str(),
        L"Catchim Launcher",
        MB_ICONERROR | MB_OK
    );
    return 1;
}
