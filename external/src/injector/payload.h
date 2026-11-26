#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Loads the embedded CatVape payload from the exe's RCDATA resource.
// Falls back to CatVape-Injectable.lua next to the exe if the resource is missing.
bool LoadEmbeddedPayload(std::vector<unsigned char>& out, std::string& outError);
