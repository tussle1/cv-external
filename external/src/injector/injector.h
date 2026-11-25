#pragma once

#include <Windows.h>
#include <string>

namespace cv::inject {

// Inject a DLL into a target process using LoadLibraryW.
// dllPath must be an absolute path accessible to the target process.
// Returns true on success. On failure, outError receives a human-readable message.
bool InjectDllLoadLibrary(DWORD pid, const std::wstring& dllPath, std::string& outError);

} // namespace cv::inject
