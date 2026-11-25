#pragma once

#include <functional>
#include <string>

namespace cv::bridge {

// Handler signature: receives payload, returns success + message.
using PayloadHandler = std::function<bool(const std::string& payload, std::string& outMessage)>;

// Creates a named pipe server, accepts one client connection, reads the payload,
// invokes handler, sends back a status response, then closes the pipe.
//
// Protocol (v1) - must match injector pipe_client:
//   client -> u32le payload_size
//   client -> payload bytes
//   server -> u32le status (0 = OK, 1 = error)
//   server -> u32le message_size
//   server -> message bytes (utf-8)
//
// Returns true if a client connected and a response was sent.
bool ServeAndHandle(const wchar_t* pipeName, const PayloadHandler& handler, std::string& outError);

} // namespace cv::bridge
