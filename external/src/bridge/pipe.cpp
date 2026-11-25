#include "pipe.h"

#include <Windows.h>
#include <cstdint>
#include <exception>
#include <sstream>
#include <vector>

namespace cv::bridge {

static std::string Win32Error(const char* what) {
    DWORD err = GetLastError();
    std::ostringstream oss;
    oss << what << " (Win32 error " << err << ")";
    return oss.str();
}

static bool ReadExact(HANDLE h, void* data, DWORD len) {
    auto* p = static_cast<unsigned char*>(data);
    DWORD total = 0;
    while (total < len) {
        DWORD read = 0;
        if (!ReadFile(h, p + total, len - total, &read, nullptr) || read == 0) return false;
        total += read;
    }
    return true;
}

static bool WriteExact(HANDLE h, const void* data, DWORD len) {
    const auto* p = static_cast<const unsigned char*>(data);
    DWORD total = 0;
    while (total < len) {
        DWORD wrote = 0;
        if (!WriteFile(h, p + total, len - total, &wrote, nullptr) || wrote == 0) return false;
        total += wrote;
    }
    return true;
}

static bool SendResponse(HANDLE pipe, uint32_t status, const std::string& message) {
    const uint32_t msgLen = static_cast<uint32_t>(message.size());
    if (!WriteExact(pipe, &status, sizeof(status))) return false;
    if (!WriteExact(pipe, &msgLen, sizeof(msgLen))) return false;
    if (msgLen > 0 && !WriteExact(pipe, message.data(), msgLen)) return false;
    FlushFileBuffers(pipe);
    return true;
}

bool ServeAndHandle(const wchar_t* pipeName, const PayloadHandler& handler, std::string& outError) {
    outError.clear();

    HANDLE pipe = CreateNamedPipeW(
        pipeName,
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        1,
        1024 * 64,
        1024 * 64,
        0,
        nullptr);

    if (pipe == INVALID_HANDLE_VALUE) {
        outError = Win32Error("CreateNamedPipeW failed");
        return false;
    }

    BOOL connected = ConnectNamedPipe(pipe, nullptr) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
    if (!connected) {
        outError = Win32Error("ConnectNamedPipe failed");
        CloseHandle(pipe);
        return false;
    }

    uint32_t len = 0;
    if (!ReadExact(pipe, &len, sizeof(len))) {
        outError = Win32Error("Read payload length failed");
        SendResponse(pipe, 1, outError);
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
        return false;
    }

    // Sanity cap: 64MB
    if (len == 0 || len > 64u * 1024u * 1024u) {
        outError = "Invalid payload length";
        SendResponse(pipe, 1, outError);
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
        return false;
    }

    std::vector<char> buf(len);
    if (!ReadExact(pipe, buf.data(), len)) {
        outError = Win32Error("Read payload failed");
        SendResponse(pipe, 1, outError);
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
        return false;
    }

    std::string payload(buf.data(), buf.size());
    std::string message;
    bool ok = false;
    try {
        ok = handler(payload, message);
    } catch (const std::exception& e) {
        ok = false;
        message = std::string("handler exception: ") + e.what();
    } catch (...) {
        ok = false;
        message = "handler threw unknown exception";
    }

    SendResponse(pipe, ok ? 0u : 1u, message);
    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
    return true;
}

} // namespace cv::bridge
