#pragma once

#include <Windows.h>
#include <TlHelp32.h>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cv::process {

// Case-insensitive wide string equality.
bool EqualsIgnoreCase(const std::wstring& a, const std::wstring& b);

// Find process IDs whose executable name matches (e.g. L"RobloxPlayerBeta.exe").
std::vector<DWORD> FindByName(const std::wstring& exeName);

// Convenience: first matching PID, if any.
std::optional<DWORD> FindFirstByName(const std::wstring& exeName);

// Wait for a process to appear, polling every pollMs up to timeoutMs.
// timeoutMs == 0 means wait forever.
std::optional<DWORD> WaitForProcess(const std::wstring& exeName, DWORD pollMs = 1000, DWORD timeoutMs = 0);

// Return the full path of a process image. Empty on failure.
std::wstring GetProcessPath(DWORD pid);

} // namespace cv::process
