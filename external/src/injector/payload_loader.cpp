#include "payload.h"

#include <Windows.h>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

#define IDR_CATVAPE_PAYLOAD 101

static std::wstring ExeDir() {
    wchar_t buf[MAX_PATH * 4]{};
    DWORD len = GetModuleFileNameW(nullptr, buf, static_cast<DWORD>(std::size(buf)));
    if (len == 0) return L".";
    fs::path p(std::wstring(buf, len));
    return p.parent_path().wstring();
}

static bool LoadFromFile(const std::wstring& path, std::vector<unsigned char>& out) {
    std::ifstream f(fs::path(path), std::ios::binary);
    if (!f) return false;
    f.seekg(0, std::ios::end);
    std::streamsize size = f.tellg();
    if (size <= 0) return false;
    f.seekg(0, std::ios::beg);
    out.resize(static_cast<size_t>(size));
    f.read(reinterpret_cast<char*>(out.data()), size);
    return f.good() || f.eof();
}

bool LoadEmbeddedPayload(std::vector<unsigned char>& out, std::string& outError) {
    out.clear();
    outError.clear();

    HRSRC res = FindResourceW(nullptr, MAKEINTRESOURCEW(IDR_CATVAPE_PAYLOAD), RT_RCDATA);
    if (res) {
        HGLOBAL handle = LoadResource(nullptr, res);
        DWORD size = SizeofResource(nullptr, res);
        void* data = handle ? LockResource(handle) : nullptr;
        if (data && size > 0) {
            auto* bytes = static_cast<const unsigned char*>(data);
            out.assign(bytes, bytes + size);
            return true;
        }
    }

    // fallback: CatVape-Injectable.lua next to exe
    std::wstring fallback = (fs::path(ExeDir()) / L"CatVape-Injectable.lua").wstring();
    if (LoadFromFile(fallback, out)) {
        return true;
    }

    outError = "payload not found in resources or next to exe";
    return false;
}
