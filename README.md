# cat vape

## how the old stuff worked
1. `api.catvape.dev/script?key=...` was the loader it never really checked the key
2. the full source was just sitting at `api.catvape.dev/contents/src` no login needed this repo has that dump
3. the only real check was `libraries/main.lua` sending the key to `api.catvape.dev/login` the server sends back lua and the client just `loadstring`s it with no check so its easy to replace
4. if it failed it did hwid checks and kicked for words like `TAMPER` `CRACKED` etc all on the client

## what was changed
- `src/libraries/main.lua` removed the login call gives premium locally and doesnt kick on auth fail
- `catvape-cracked-loader.lua` offline loader puts the bundled `src/` into the executor workspace and runs from local files no requests to `api.catvape.dev`

## how to use
1. load the whole `src` folder into your executor workspace so `src/...` paths work or run the loader from the folder that has `src`
2. run `catvape-cracked-loader.lua`

## stuff found
- **big:** source endpoint (`/contents/src`) had no auth so anyone could grab it
- **big:** auth callback was unsigned lua from server and client just ran it
- **medium:** hwid and tamper checks were all client side easy to patch
- **low:** commit pinning was just a plain text file easy to fake

this is not the full cracked version totally

## catvape external (injector project)

`external/` has a windows c++ project that bakes in `CatVape-Injectable.lua` and injects it into roblox with a bridge dll

- `CatVape-Injectable.lua` one file injectable build made from `src/` just paste into any executor
- `external/` windows injector project (`CatVapeExternal.exe` + `CatVapeBridge.dll`)

build it on windows

```bat
cd external
build.bat
```

the part that actually runs the lua inside roblox is left open at `external/src/bridge/execute.cpp` you gotta fill that in
check `external/README.md` for the rest

make the files again after changing `src/`

```bash
python3 scripts/build_injectable.py
python3 external/tools/embed_payload.py
```
