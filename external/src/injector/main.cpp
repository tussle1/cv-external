#include "injector.h"
#include "payload.h"
#include "pipe_client.h"
#include "process.h"

#include <Windows.h>

#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

static constexpr wchar_t kRobloxExe[] = L"RobloxPlayerBeta.exe";
static constexpr wchar_t kBridgeDll[] = L"CatVapeBridge.dll";
static constexpr wchar_t kPipeName[]  = L"\\\\.\\pipe\\CatVapeExternal_v1";

static std::wstring ExeDir() {
    wchar_t buf[MAX_PATH * 4]{};
    DWORD len = GetModuleFileNameW(nullptr, buf, static_cast<DWORD>(std::size(buf)));
    if (len == 0) return L".";
    fs::path p(std::wstring(buf, len));
    return p.parent_path().wstring();
}

static void PrintBanner() {
    std::cout
        << "==============================================\n"
        << "  CatVape External - injector/loader\n"
        << "  branch: arena/f7918408-catvape-external\n"
        << "==============================================\n\n";
}

int wmain() {
    PrintBanner();

    const std::wstring dllPath = (fs::path(ExeDir()) / kBridgeDll).wstring();
    if (!fs::exists(dllPath)) {
        std::wcerr << L"[!] Bridge DLL not found next to exe:\n    " << dllPath << L"\n\n"
                   << L"    Put CatVapeBridge.dll in the same folder as CatVapeExternal.exe.\n";
        return 1;
    }

    std::cout << "[*] Looking for " << "RobloxPlayerBeta.exe" << "...\n";
    auto pidOpt = cv::process::FindFirstByName(kRobloxExe);
    if (!pidOpt) {
        std::cout << "[*] Roblox not running. Waiting for it (Ctrl+C to cancel)...\n";
        pidOpt = cv::process::WaitForProcess(kRobloxExe, 1000, 0);
    }
    if (!pidOpt) {
        std::cerr << "[!] Could not find RobloxPlayerBeta.exe.\n";
        return 1;
    }

    DWORD pid = *pidOpt;
    std::wcout << L"[+] Found Roblox PID: " << pid << L"\n";
    std::wstring procPath = cv::process::GetProcessPath(pid);
    if (!procPath.empty()) {
        std::wcout << L"    " << procPath << L"\n";
    }

    std::cout << "[*] Injecting bridge DLL...\n";
    std::string err;
    if (!cv::inject::InjectDllLoadLibrary(pid, dllPath, err)) {
        std::cerr << "[!] Injection failed: " << err << "\n";
        std::cerr << "    Common causes: wrong architecture, AV interference, missing DLL deps,\n"
                  << "    or Roblox running as a different integrity level.\n";
        return 1;
    }
    std::cout << "[+] DLL injected.\n";

    std::cout << "[*] Waiting for bridge pipe and sending payload ("
              << kCatVapePayloadSize << " bytes)...\n";

    std::string msg;
    bool ok = cv::pipe::SendPayload(kPipeName, kCatVapePayload, kCatVapePayloadSize, msg, 20000);
    if (!ok) {
        std::cerr << "[!] Payload delivery failed: " << msg << "\n";
        std::cerr << "    The DLL may be injected but the execution bridge is stubbed or crashed.\n"
                  << "    Check external/src/bridge/execute.cpp.\n";
        return 1;
    }

    std::cout << "[+] Bridge response: " << (msg.empty() ? "ok" : msg) << "\n";
    std::cout << "[+] Done. If the bridge executed the payload, CatVape should be live in-game.\n";
    return 0;
}
