#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace cv::pipe {

// Sends the payload to the injected bridge DLL over a named pipe.
// Protocol (v1):
//   -> u32le payload_size
//   -> payload bytes
//   <- u32le status (0 = OK, 1 = error)
//   <- u32le message_size
//   <- message bytes (utf-8), may be empty
//
// Returns true on OK status. outMessage receives status/error text.
bool SendPayload(const wchar_t* pipeName,
                 const unsigned char* payload,
                 std::size_t payloadSize,
                 std::string& outMessage,
                 int timeoutMs = 15000);

} // namespace cv::pipe
