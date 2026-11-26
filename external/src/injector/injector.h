#pragma once

#include <Windows.h>
#include <string>

namespace cv::inject {

// Inject a DLL into a target process using manual mapping.
// dllPath must be an absolute path to the DLL on disk.
// Returns true on success. On failure, outError receives a human-readable message.
bool InjectDll(DWORD pid, const std::wstring& dllPath, std::string& outError);

} // namespace cv::inject
