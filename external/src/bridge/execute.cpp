#include "execute.h"

#include <Windows.h>
#include <sstream>

namespace cv::bridge {

bool ExecuteLuau(const std::string& script, std::string& outError) {
    // ------------------------------------------------------------
    // STUB: wire your Luau execution bridge here.
    //
    // Examples of what goes in this function (you implement):
    //   - resolve Roblox Luau state / task scheduler offsets
    //   - call your internal getglobal/loadstring/luau_load bridge
    //   - enqueue the script on the Roblox scheduler
    //
    // This stub intentionally does NOT execute anything. It only
    // proves the pipe/injection/payload path is working.
    // ------------------------------------------------------------

    if (script.empty()) {
        outError = "execute.cpp: empty script";
        return false;
    }

    // Optional: drop the payload next to the DLL for debugging.
    // Comment this out if you don't want the script written to disk.
    try {
        char modulePath[MAX_PATH * 4]{};
        HMODULE self = nullptr;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                   GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCSTR>(&ExecuteLuau), &self)) {
            DWORD len = GetModuleFileNameA(self, modulePath, static_cast<DWORD>(sizeof(modulePath)));
            if (len > 0) {
                std::string path(modulePath, len);
                auto pos = path.find_last_of("\\/");
                std::string dir = (pos == std::string::npos) ? "." : path.substr(0, pos);
                std::string outPath = dir + "\\CatVape-Payload-Received.lua";
                HANDLE f = CreateFileA(outPath.c_str(), GENERIC_WRITE, 0, nullptr,
                                       CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (f != INVALID_HANDLE_VALUE) {
                    DWORD written = 0;
                    WriteFile(f, script.data(), static_cast<DWORD>(script.size()), &written, nullptr);
                    CloseHandle(f);
                }
            }
        }
    } catch (...) {
        // Non-fatal: debug dump failed, execution bridge still stubbed.
    }

    std::ostringstream oss;
    oss << "bridge stub: received " << script.size()
        << " bytes but ExecuteLuau is not implemented. "
        << "Implement cv::bridge::ExecuteLuau in external/src/bridge/execute.cpp.";
    outError = oss.str();
    return false;
}

} // namespace cv::bridge
