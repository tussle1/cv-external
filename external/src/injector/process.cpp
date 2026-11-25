#include "process.h"

#include <algorithm>
#include <cwctype>

namespace cv::process {

bool EqualsIgnoreCase(const std::wstring& a, const std::wstring& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::towlower(a[i]) != std::towlower(b[i])) return false;
    }
    return true;
}

std::vector<DWORD> FindByName(const std::wstring& exeName) {
    std::vector<DWORD> pids;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return pids;

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snap, &entry)) {
        do {
            if (EqualsIgnoreCase(entry.szExeFile, exeName)) {
                pids.push_back(entry.th32ProcessID);
            }
        } while (Process32NextW(snap, &entry));
    }

    CloseHandle(snap);
    return pids;
}

std::optional<DWORD> FindFirstByName(const std::wstring& exeName) {
    auto pids = FindByName(exeName);
    if (pids.empty()) return std::nullopt;
    return pids.front();
}

std::optional<DWORD> WaitForProcess(const std::wstring& exeName, DWORD pollMs, DWORD timeoutMs) {
    DWORD waited = 0;
    while (true) {
        if (auto pid = FindFirstByName(exeName)) return pid;
        if (timeoutMs != 0 && waited >= timeoutMs) return std::nullopt;
        Sleep(pollMs);
        if (timeoutMs != 0) waited += pollMs;
    }
}

std::wstring GetProcessPath(DWORD pid) {
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return L"";

    wchar_t buf[MAX_PATH * 4]{};
    DWORD size = static_cast<DWORD>(std::size(buf));
    std::wstring out;
    if (QueryFullProcessImageNameW(h, 0, buf, &size)) {
        out.assign(buf, size);
    }
    CloseHandle(h);
    return out;
}

} // namespace cv::process
