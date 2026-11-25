# catvape external

this is a windows project that puts the catvape script into roblox with its own injector exe and dll

its not some magic undetected thing the injector and pipe stuff is done but the part that actually runs the lua inside roblox is left open for you to fill in

## what you get

- `CatVapeExternal.exe` finds roblox injects `CatVapeBridge.dll` and sends the catvape script through a pipe
- `CatVapeBridge.dll` gets injected into roblox gets the script and sends it to `ExecuteLuau(...)`

## how to build

you need cmake and visual studio build tools on windows

```bat
cd external
build.bat
```

or do it yourself

```bat
cd external
cmake -S . -B build -A x64
cmake --build build --config Release
```

the files show up in `external/build/bin/`

if you use mingw

```bat
cd external
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## how to use

1. build it
2. put `CatVapeExternal.exe` and `CatVapeBridge.dll` in the same folder
3. open roblox and join a game
4. run `CatVapeExternal.exe`
5. it should find roblox inject the dll and send the script

## files

```
external/
  CMakeLists.txt
  build.bat
  README.md
  src/
    injector/
      main.cpp          starts everything
      process.*         finds roblox
      injector.*        does the dll injection
      pipe_client.*     sends the script to the dll
      payload.h         the catvape script baked in
    bridge/
      dllmain.cpp       dll start
      bridge.*          runs the pipe and handles the script
      pipe.*            pipe server
      execute.*         this is the part you need to fill in
  tools/
    embed_payload.py    remakes payload.h from CatVape-Injectable.lua
```

## the part you gotta fill in

the only part thats not done is `external/src/bridge/execute.cpp`

```cpp
bool ExecuteLuau(const std::string& script, std::string& outError);
```

that function gets the whole catvape script
you gotta make it run that script inside roblox

when roblox updates this is probably the file you change

the injector pipe and payload stuff should be fine

## remake the payload

if you change stuff in `src/` or rebuild the injectable run this

```bash
python3 scripts/build_injectable.py
python3 external/tools/embed_payload.py
```

then build the c++ project again

## notes

- injection uses LoadLibraryW its simple and easy to swap out later if you want
- the pipe name is `\\.\pipe\CatVapeExternal_v1`
- this keeps you from having a known executor like real or potassium sitting around but its still an injector and dll so be smart
- i didnt include a built exe you build it yourself on windows
