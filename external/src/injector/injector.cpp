#include "injector.h"

#include <sstream>

namespace cv::inject {

static std::string Win32Error(const char* what) {
    DWORD err = GetLastError();
    std::ostringstream oss;
    oss << what << " (Win32 error " << err << ")";
    return oss.str();
}

bool InjectDllLoadLibrary(DWORD pid, const std::wstring& dllPath, std::string& outError) {
    if (dllPath.empty()) {
        outError = "dllPath is empty";
        return false;
    }

    HANDLE proc = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
        FALSE, pid);
    if (!proc) {
        outError = Win32Error("OpenProcess failed");
        return false;
    }

    const size_t bytes = (dllPath.size() + 1) * sizeof(wchar_t);
    LPVOID remoteMem = VirtualAllocEx(proc, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteMem) {
        outError = Win32Error("VirtualAllocEx failed");
        CloseHandle(proc);
        return false;
    }

    if (!WriteProcessMemory(proc, remoteMem, dllPath.c_str(), bytes, nullptr)) {
        outError = Win32Error("WriteProcessMemory failed");
        VirtualFreeEx(proc, remoteMem, 0, MEM_RELEASE);
        CloseHandle(proc);
        return false;
    }

    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    if (!kernel32) {
        outError = Win32Error("GetModuleHandleW(kernel32) failed");
        VirtualFreeEx(proc, remoteMem, 0, MEM_RELEASE);
        CloseHandle(proc);
        return false;
    }

    auto loadLib = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        GetProcAddress(kernel32, "LoadLibraryW"));
    if (!loadLib) {
        outError = Win32Error("GetProcAddress(LoadLibraryW) failed");
        VirtualFreeEx(proc, remoteMem, 0, MEM_RELEASE);
        CloseHandle(proc);
        return false;
    }

    HANDLE thread = CreateRemoteThread(proc, nullptr, 0, loadLib, remoteMem, 0, nullptr);
    if (!thread) {
        outError = Win32Error("CreateRemoteThread failed");
        VirtualFreeEx(proc, remoteMem, 0, MEM_RELEASE);
        CloseHandle(proc);
        return false;
    }

    WaitForSingleObject(thread, 10000);

    DWORD exitCode = 0;
    GetExitCodeThread(thread, &exitCode);

    CloseHandle(thread);
    VirtualFreeEx(proc, remoteMem, 0, MEM_RELEASE);
    CloseHandle(proc);

    // LoadLibraryW returns HMODULE; 0 means failure in the remote process.
    if (exitCode == 0) {
        outError = "Remote LoadLibraryW returned NULL (DLL failed to load in target process)";
        return false;
    }

    return true;
}

} // namespace cv::inject
