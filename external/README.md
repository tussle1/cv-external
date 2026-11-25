# CatVape External

A Windows injector project that embeds the CatVape Luau payload and delivers it into a running Roblox process through an injected bridge DLL.

This is **option 2**: a complete, buildable project with a clean execution-bridge seam. It is not a magic one-click undetected executor. The injector/pipe/payload side is stable; the Luau execution bridge inside the DLL is the part you keep current against Roblox updates.

## What you get

- `CatVapeExternal.exe` - injector/loader. Finds `RobloxPlayerBeta.exe`, injects `CatVapeBridge.dll`, sends the embedded CatVape payload over a named pipe.
- `CatVapeBridge.dll` - injected into Roblox. Receives the payload and hands it to `ExecuteLuau(...)`.

## Build (Windows)

### Visual Studio / Build Tools

```bat
cd external
build.bat
```

Or manually:

```bat
cd external
cmake -S . -B build -A x64
cmake --build build --config Release
```

Outputs land in `external/build/bin/`.

### MinGW-w64

```bat
cd external
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Use

1. Build the project.
2. Put `CatVapeExternal.exe` and `CatVapeBridge.dll` in the same folder.
3. Launch Roblox and join a game.
4. Run `CatVapeExternal.exe` as the same user (admin not required for standard LoadLibrary injection into your own-user process, but some environments may differ).
5. The injector finds Roblox, injects the bridge DLL, and sends the embedded CatVape payload.

## Project layout

```
external/
  CMakeLists.txt
  build.bat
  README.md
  src/
    injector/
      main.cpp          - CLI entrypoint
      process.*         - Roblox process discovery
      injector.*        - LoadLibraryW DLL injection
      pipe_client.*     - named pipe client, sends payload
      payload.h         - embedded CatVape-Injectable.lua (generated)
    bridge/
      dllmain.cpp       - DLL entrypoint
      bridge.*          - pipe server + payload dispatch
      pipe.*            - named pipe server
      execute.*         - *** EXECUTION BRIDGE SEAM ***
  tools/
    embed_payload.py    - regenerates payload.h from CatVape-Injectable.lua
```

## The execution bridge seam

The only part that is intentionally stubbed is `external/src/bridge/execute.cpp`.

```cpp
bool ExecuteLuau(const std::string& script);
```

This function receives the full CatVape payload. You wire it to your Luau execution method (Roblox Luau VM hook, internal bridge, etc.). When Roblox updates, this is the file you touch.

The pipe/injection/payload plumbing does not need to change for that.

## Regenerating the embedded payload

If you change `src/` or rebuild the injectable, regenerate the header:

```bash
python3 scripts/build_injectable.py
python3 external/tools/embed_payload.py
```

Then rebuild the C++ project.

## Notes

- **Injection method**: LoadLibraryW. Simple, readable, easy to swap. If you want manual mapping, replace `injector.cpp` - the rest of the project stays the same.
- **Payload delivery**: named pipe `\\.\pipe\CatVapeExternal`. The DLL spins up the pipe server on inject; the exe connects and sends `[u32 length][payload bytes]`.
- **Detection surface**: this avoids shipping a known executor binary like Real/Potassium, but it is still an injector+DLL. Be smart about what you do with it. The bridge implementation you plug in determines most of the runtime behavior.
- **No precompiled binary in this repo**: build it yourself on Windows so you know what's in it.
