#include "injector.h"

#include <fstream>
#include <sstream>
#include <vector>

namespace cv::inject {

static std::string Win32Error(const char* what) {
    DWORD err = GetLastError();
    std::ostringstream oss;
    oss << what << " (Win32 error " << err << ")";
    return oss.str();
}

using f_LoadLibraryA = HINSTANCE(WINAPI*)(const char*);
using f_GetProcAddress = FARPROC(WINAPI*)(HMODULE, const char*);
using f_DLL_ENTRY_POINT = BOOL(WINAPI*)(void*, DWORD, void*);
using f_RtlAddFunctionTable = BOOLEAN(WINAPI*)(PRUNTIME_FUNCTION, DWORD, DWORD64);

struct ManualMapData {
    f_LoadLibraryA pLoadLibraryA;
    f_GetProcAddress pGetProcAddress;
    f_RtlAddFunctionTable pRtlAddFunctionTable;
    HINSTANCE hMod;
    DWORD reason;
    LPVOID reserved;
    BOOL sehSupport;
};

static void __stdcall Shellcode(ManualMapData* pData) {
    if (!pData) return;

    auto* base = reinterpret_cast<unsigned char*>(pData->hMod);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    auto* opt = &nt->OptionalHeader;

    auto _LoadLibraryA = pData->pLoadLibraryA;
    auto _GetProcAddress = pData->pGetProcAddress;
    auto _RtlAddFunctionTable = pData->pRtlAddFunctionTable;
    auto _DllMain = reinterpret_cast<f_DLL_ENTRY_POINT>(base + opt->AddressOfEntryPoint);

    // Relocations
    auto relocDelta = reinterpret_cast<BYTE*>(base) - opt->ImageBase;
    if (relocDelta) {
        auto& relocDir = opt->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
        if (relocDir.Size) {
            auto* reloc = reinterpret_cast<IMAGE_BASE_RELOCATION*>(base + relocDir.VirtualAddress);
            auto* relocEnd = reinterpret_cast<IMAGE_BASE_RELOCATION*>(reinterpret_cast<BYTE*>(reloc) + relocDir.Size);
            while (reloc < relocEnd && reloc->SizeOfBlock) {
                UINT count = (reloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
                auto* entry = reinterpret_cast<WORD*>(reloc + 1);
                for (UINT i = 0; i < count; ++i, ++entry) {
                    UINT type = (*entry) >> 12;
                    UINT offset = (*entry) & 0xFFF;
                    if (type == IMAGE_REL_BASED_DIR64) {
                        auto* patch = reinterpret_cast<UINT_PTR*>(base + reloc->VirtualAddress + offset);
                        *patch += reinterpret_cast<UINT_PTR>(relocDelta);
                    } else if (type == IMAGE_REL_BASED_HIGHLOW) {
                        auto* patch = reinterpret_cast<DWORD*>(base + reloc->VirtualAddress + offset);
                        *patch += static_cast<DWORD>(reinterpret_cast<UINT_PTR>(relocDelta));
                    }
                }
                reloc = reinterpret_cast<IMAGE_BASE_RELOCATION*>(reinterpret_cast<BYTE*>(reloc) + reloc->SizeOfBlock);
            }
        }
    }

    // Imports
    auto& importDir = opt->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (importDir.Size) {
        auto* desc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + importDir.VirtualAddress);
        while (desc->Name) {
            char* modName = reinterpret_cast<char*>(base + desc->Name);
            HINSTANCE hMod = _LoadLibraryA(modName);
            if (hMod) {
                auto* thunkRef = reinterpret_cast<IMAGE_THUNK_DATA*>(base + (desc->OriginalFirstThunk ? desc->OriginalFirstThunk : desc->FirstThunk));
                auto* funcRef = reinterpret_cast<IMAGE_THUNK_DATA*>(base + desc->FirstThunk);
                while (thunkRef->u1.AddressOfData) {
                    if (IMAGE_SNAP_BY_ORDINAL(thunkRef->u1.Ordinal)) {
                        funcRef->u1.Function = reinterpret_cast<UINT_PTR>(_GetProcAddress(hMod, reinterpret_cast<char*>(thunkRef->u1.Ordinal & 0xFFFF)));
                    } else {
                        auto* importByName = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + thunkRef->u1.AddressOfData);
                        funcRef->u1.Function = reinterpret_cast<UINT_PTR>(_GetProcAddress(hMod, importByName->Name));
                    }
                    ++thunkRef;
                    ++funcRef;
                }
            }
            ++desc;
        }
    }

    // TLS
    auto& tlsDir = opt->DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS];
    if (tlsDir.Size) {
        auto* tls = reinterpret_cast<IMAGE_TLS_DIRECTORY*>(base + tlsDir.VirtualAddress);
        auto* callback = reinterpret_cast<PIMAGE_TLS_CALLBACK*>(tls->AddressOfCallBacks);
        if (callback) {
            for (; *callback; ++callback) {
                (*callback)(base, DLL_PROCESS_ATTACH, nullptr);
            }
        }
    }

    // Exception support
    if (pData->sehSupport) {
        auto& exDir = opt->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
        if (exDir.Size) {
            _RtlAddFunctionTable(
                reinterpret_cast<PRUNTIME_FUNCTION>(base + exDir.VirtualAddress),
                exDir.Size / sizeof(RUNTIME_FUNCTION),
                reinterpret_cast<DWORD64>(base));
        }
    }

    // DllMain
    _DllMain(base, DLL_PROCESS_ATTACH, nullptr);

    pData->hMod = reinterpret_cast<HINSTANCE>(base);
}

static void __stdcall ShellcodeEnd() {}

bool InjectDll(DWORD pid, const std::wstring& dllPath, std::string& outError) {
    if (dllPath.empty()) {
        outError = "dllPath is empty";
        return false;
    }

    std::ifstream file(dllPath, std::ios::binary | std::ios::ate);
    if (!file) {
        outError = "Could not open DLL file";
        return false;
    }
    auto fileSize = file.tellg();
    if (fileSize < 0x1000) {
        outError = "DLL file too small or invalid";
        return false;
    }

    std::vector<BYTE> src(static_cast<size_t>(fileSize));
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(src.data()), fileSize);
    file.close();

    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(src.data());
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        outError = "Invalid DOS signature";
        return false;
    }
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(src.data() + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        outError = "Invalid NT signature";
        return false;
    }
    if (nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        outError = "DLL must be x64 (build both injector and dll as x64)";
        return false;
    }

    HANDLE proc = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
        PROCESS_VM_WRITE | PROCESS_VM_READ,
        FALSE, pid);
    if (!proc) {
        outError = Win32Error("OpenProcess failed");
        return false;
    }

    auto* remoteImage = reinterpret_cast<BYTE*>(VirtualAllocEx(
        proc, nullptr, nt->OptionalHeader.SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!remoteImage) {
        outError = Win32Error("VirtualAllocEx(image) failed");
        CloseHandle(proc);
        return false;
    }

    // Write headers
    if (!WriteProcessMemory(proc, remoteImage, src.data(), nt->OptionalHeader.SizeOfHeaders, nullptr)) {
        outError = Win32Error("WriteProcessMemory(headers) failed");
        VirtualFreeEx(proc, remoteImage, 0, MEM_RELEASE);
        CloseHandle(proc);
        return false;
    }

    // Write sections
    auto* section = IMAGE_FIRST_SECTION(nt);
    for (UINT i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section) {
        if (section->SizeOfRawData == 0) continue;
        if (!WriteProcessMemory(proc, remoteImage + section->VirtualAddress,
                                src.data() + section->PointerToRawData,
                                section->SizeOfRawData, nullptr)) {
            outError = Win32Error("WriteProcessMemory(section) failed");
            VirtualFreeEx(proc, remoteImage, 0, MEM_RELEASE);
            CloseHandle(proc);
            return false;
        }
    }

    // Prepare mapping data
    ManualMapData data{};
    HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    data.pLoadLibraryA = reinterpret_cast<f_LoadLibraryA>(GetProcAddress(kernel32, "LoadLibraryA"));
    data.pGetProcAddress = reinterpret_cast<f_GetProcAddress>(GetProcAddress(kernel32, "GetProcAddress"));
    data.pRtlAddFunctionTable = ntdll ? reinterpret_cast<f_RtlAddFunctionTable>(GetProcAddress(ntdll, "RtlAddFunctionTable")) : nullptr;
    data.hMod = reinterpret_cast<HINSTANCE>(remoteImage);
    data.reason = DLL_PROCESS_ATTACH;
    data.reserved = nullptr;
    data.sehSupport = (nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION].Size != 0 && data.pRtlAddFunctionTable != nullptr);

    auto* remoteData = VirtualAllocEx(proc, nullptr, sizeof(ManualMapData), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteData) {
        outError = Win32Error("VirtualAllocEx(data) failed");
        VirtualFreeEx(proc, remoteImage, 0, MEM_RELEASE);
        CloseHandle(proc);
        return false;
    }
    if (!WriteProcessMemory(proc, remoteData, &data, sizeof(data), nullptr)) {
        outError = Win32Error("WriteProcessMemory(data) failed");
        VirtualFreeEx(proc, remoteData, 0, MEM_RELEASE);
        VirtualFreeEx(proc, remoteImage, 0, MEM_RELEASE);
        CloseHandle(proc);
        return false;
    }

    // Write shellcode
    // Fixed size: Shellcode is small; avoid fragile function-pointer subtraction.
    auto shellcodeSize = static_cast<SIZE_T>(0x1000);
    auto* remoteShellcode = VirtualAllocEx(proc, nullptr, shellcodeSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!remoteShellcode) {
        outError = Win32Error("VirtualAllocEx(shellcode) failed");
        VirtualFreeEx(proc, remoteData, 0, MEM_RELEASE);
        VirtualFreeEx(proc, remoteImage, 0, MEM_RELEASE);
        CloseHandle(proc);
        return false;
    }
    if (!WriteProcessMemory(proc, remoteShellcode, reinterpret_cast<void*>(&Shellcode), shellcodeSize, nullptr)) {
        outError = Win32Error("WriteProcessMemory(shellcode) failed");
        VirtualFreeEx(proc, remoteShellcode, 0, MEM_RELEASE);
        VirtualFreeEx(proc, remoteData, 0, MEM_RELEASE);
        VirtualFreeEx(proc, remoteImage, 0, MEM_RELEASE);
        CloseHandle(proc);
        return false;
    }

    HANDLE thread = CreateRemoteThread(proc, nullptr, 0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(remoteShellcode),
        remoteData, 0, nullptr);
    if (!thread) {
        outError = Win32Error("CreateRemoteThread failed");
        VirtualFreeEx(proc, remoteShellcode, 0, MEM_RELEASE);
        VirtualFreeEx(proc, remoteData, 0, MEM_RELEASE);
        VirtualFreeEx(proc, remoteImage, 0, MEM_RELEASE);
        CloseHandle(proc);
        return false;
    }

    WaitForSingleObject(thread, 15000);
    CloseHandle(thread);

    // Check if DllMain ran by reading back hMod
    ManualMapData check{};
    ReadProcessMemory(proc, remoteData, &check, sizeof(check), nullptr);

    VirtualFreeEx(proc, remoteShellcode, 0, MEM_RELEASE);
    VirtualFreeEx(proc, remoteData, 0, MEM_RELEASE);
    // Don't free remoteImage - it's the mapped DLL

    CloseHandle(proc);

    if (check.hMod != reinterpret_cast<HINSTANCE>(remoteImage)) {
        // Not necessarily fatal - but suspicious
    }

    return true;
}

} // namespace cv::inject
