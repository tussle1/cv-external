#include "bridge.h"
#include "execute.h"
#include "pipe.h"

#include <Windows.h>
#include <string>

namespace cv::bridge {

static constexpr wchar_t kPipeName[] = L"\\\\.\\pipe\\CatVapeExternal_v1";

static void DebugLog(const std::string& msg) {
    std::string line = "[CatVapeBridge] " + msg + "\n";
    OutputDebugStringA(line.c_str());
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
