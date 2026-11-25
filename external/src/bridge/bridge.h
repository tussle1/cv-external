#pragma once

namespace cv::bridge {

// Starts the bridge: creates the named pipe server, waits for a payload,
// dispatches to ExecuteLuau, responds to the injector. Intended to run on
// a dedicated thread (call from DllMain's thread is not allowed for heavy work).
void RunBridge();

} // namespace cv::bridge
