#include "bridge.h"

#include <Windows.h>
#include <fstream>
#include <string>

static std::string DllDir(HMODULE hModule) {
    char buf[MAX_PATH * 4]{};
    DWORD len = GetModuleFileNameA(hModule, buf, static_cast<DWORD>(sizeof(buf)));
    if (len > 0) {
        std::string path(buf, len);
        auto pos = path.find_last_of("\\/");
        return (pos == std::string::npos) ? "." : path.substr(0, pos);
    }
    return ".";
}

static void Log(HMODULE hModule, const char* msg) {
    try {
        std::ofstream f(DllDir(hModule) + "\\CatVapeBridge.log", std::ios::app);
        if (f) f << "[CatVapeBridge] " << msg << "\n";
    } catch (...) {}
}

static DWORD WINAPI BridgeThread(LPVOID) {
    cv::bridge::RunBridge();
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID /*reserved*/) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        Log(hModule, "dll attached");
        HANDLE t = CreateThread(nullptr, 0, BridgeThread, nullptr, 0, nullptr);
        if (t) {
            CloseHandle(t);
            Log(hModule, "bridge thread created");
        } else {
            Log(hModule, "failed to create bridge thread");
        }
    }
    return TRUE;
}
