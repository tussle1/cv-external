#pragma once

#include <string>

// ============================================================
// EXECUTION BRIDGE SEAM
// ============================================================
// This is the ONLY part you need to implement for your environment.
//
// ExecuteLuau receives the full CatVape payload (the embedded
// CatVape-Injectable.lua contents) as a UTF-8 string.
//
// Your job: execute that Luau script inside the Roblox client's
// Luau VM. How you do that depends on your bridge/exploit method
// and will need updating when Roblox updates.
//
// Return true on success. On failure, set outError with a short
// diagnostic message - it gets sent back to the injector exe.
//
// DO NOT put injector/pipe logic here. Keep this file focused.
// ============================================================

namespace cv::bridge {

bool ExecuteLuau(const std::string& script, std::string& outError);

} // namespace cv::bridge
