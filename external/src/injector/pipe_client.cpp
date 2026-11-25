#include "pipe_client.h"

#include <Windows.h>

#include <sstream>
#include <vector>

namespace cv::pipe {

static std::string Win32Error(const char* what) {
    DWORD err = GetLastError();
    std::ostringstream oss;
    oss << what << " (Win32 error " << err << ")";
    return oss.str();
}

static bool WriteExact(HANDLE h, const void* data, DWORD len) {
    const auto* p = static_cast<const unsigned char*>(data);
    DWORD total = 0;
    while (total < len) {
        DWORD wrote = 0;
        if (!WriteFile(h, p + total, len - total, &wrote, nullptr) || wrote == 0) {
            return false;
        }
        total += wrote;
    }
    return true;
}

static bool ReadExact(HANDLE h, void* data, DWORD len) {
    auto* p = static_cast<unsigned char*>(data);
    DWORD total = 0;
    while (total < len) {
        DWORD read = 0;
        if (!ReadFile(h, p + total, len - total, &read, nullptr) || read == 0) {
            return false;
        }
        total += read;
    }
    return true;
}

bool SendPayload(const wchar_t* pipeName,
                 const unsigned char* payload,
                 std::size_t payloadSize,
                 std::string& outMessage,
                 int timeoutMs) {
    if (!payload || payloadSize == 0) {
        outMessage = "empty payload";
        return false;
    }

    const DWORD deadline = GetTickCount() + static_cast<DWORD>(timeoutMs < 0 ? 0 : timeoutMs);
    while (true) {
        if (WaitNamedPipeW(pipeName, 500)) break;
        DWORD err = GetLastError();
        if (err != ERROR_SEM_TIMEOUT && err != ERROR_FILE_NOT_FOUND) {
            outMessage = Win32Error("WaitNamedPipeW failed");
            return false;
        }
        if (static_cast<int>(GetTickCount() - deadline) >= 0) {
            outMessage = "Timed out waiting for bridge pipe. Is the DLL injected and running?";
            return false;
        }
        Sleep(250);
    }

    HANDLE pipe = CreateFileW(
        pipeName,
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr);

    if (pipe == INVALID_HANDLE_VALUE) {
        outMessage = Win32Error("CreateFileW(pipe) failed");
        return false;
    }

    DWORD mode = PIPE_READMODE_BYTE;
    SetNamedPipeHandleState(pipe, &mode, nullptr, nullptr);

    const uint32_t len = static_cast<uint32_t>(payloadSize);
    if (!WriteExact(pipe, &len, sizeof(len)) || !WriteExact(pipe, payload, static_cast<DWORD>(payloadSize))) {
        outMessage = Win32Error("WriteFile(pipe) failed");
        CloseHandle(pipe);
        return false;
    }
    FlushFileBuffers(pipe);

    uint32_t status = 1;
    uint32_t msgLen = 0;
    if (!ReadExact(pipe, &status, sizeof(status)) || !ReadExact(pipe, &msgLen, sizeof(msgLen))) {
        outMessage = Win32Error("ReadFile(pipe response) failed");
        CloseHandle(pipe);
        return false;
    }

    std::string msg;
    if (msgLen > 0) {
        std::vector<char> buf(msgLen);
        if (!ReadExact(pipe, buf.data(), msgLen)) {
            outMessage = Win32Error("ReadFile(pipe message) failed");
            CloseHandle(pipe);
            return false;
        }
        msg.assign(buf.data(), buf.size());
    }

    CloseHandle(pipe);
    outMessage = msg;
    return status == 0;
}

} // namespace cv::pipe
