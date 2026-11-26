#include "bridge.h"
#include "execute.h"
#include "pipe.h"

#include <Windows.h>
#include <fstream>
#include <string>

namespace cv::bridge {

static constexpr wchar_t kPipeName[] = L"\\\\.\\pipe\\CatVapeExternal_v1";

static std::string DllDir() {
    char buf[MAX_PATH * 4]{};
    HMODULE self = nullptr;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(&DllDir), &self)) {
        DWORD len = GetModuleFileNameA(self, buf, static_cast<DWORD>(sizeof(buf)));
        if (len > 0) {
            std::string path(buf, len);
            auto pos = path.find_last_of("\\/");
            return (pos == std::string::npos) ? "." : path.substr(0, pos);
        }
    }
    return ".";
}

static void DebugLog(const std::string& msg) {
    std::string line = "[CatVapeBridge] " + msg + "\n";
    OutputDebugStringA(line.c_str());
    try {
        std::ofstream f(DllDir() + "\\CatVapeBridge.log", std::ios::app);
        if (f) f << line;
    } catch (...) {
    }
}

void RunBridge() {
    DebugLog("bridge thread started, waiting for injector payload...");

    std::string err;
    bool served = ServeAndHandle(kPipeName, [](const std::string& payload, std::string& outMessage) -> bool {
        DebugLog("payload received, size=" + std::to_string(payload.size()));
        std::string execErr;
        bool ok = ExecuteLuau(payload, execErr);
        outMessage = ok ? "executed" : execErr;
        DebugLog(ok ? "execution reported success" : ("execution failed: " + execErr));
        return ok;
    }, err);

    if (!served) {
        DebugLog("ServeAndHandle failed: " + err);
    } else {
        DebugLog("bridge session complete");
    }
}

} // namespace cv::bridge
