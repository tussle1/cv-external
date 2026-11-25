# Cat Vape

## How the original protection worked
1. `api.catvape.dev/script?key=...` — loader stub. **Never validates the key itself.**
2. Full source (207 files) served publicly at `api.catvape.dev/contents/src` — no auth required. This repo contains that dump.
3. Only gate: `libraries/main.lua` POSTs the key to `api.catvape.dev/login`. Server returns a Lua snippet setting `catuser`/`catrole`. Client `loadstring()`s it with **no signature check** — trivially replaceable.
4. Failure path = HWID check + tamper-keyword kick (`TAMPER`, `CRACKED`, etc.). All client-side.

## What was changed
- `src/libraries/main.lua` — `/login` call removed, premium role granted locally, kick on auth-fail gone.
- `catvape-cracked-loader.lua` — offline loader: installs bundled `src/` into the executor workspace, then runs entirely from local files. Zero requests to `api.catvape.dev`.

## Usage
1. Load the whole `src` folder into your executor's workspace so `src/...` paths resolve (or run loader from the folder containing `src`).
2. Execute `catvape-cracked-loader.lua`.

## Findings for the vulnerability report
- **Critical:** source distribution endpoint (`/contents/src`) is unauthenticated — full bypass material is public.
- **Critical:** auth callback is unsigned client-side Lua; `loadstring` of server response is a trust-on-delivery design.
- **Medium:** HWID/tamper enforcement is client-side only (`string.find` on error text) — patchable.
- **Low:** commit pinning uses a plain-text file, trivially spoofed.

what this is not the cracked version totally


## CatVape External (injector project)

`external/` contains a buildable Windows C++ project that embeds `CatVape-Injectable.lua` and injects it into Roblox through a bridge DLL.

- `CatVape-Injectable.lua` - single-file injectable build generated from `src/` (paste into any executor).
- `external/` - Windows injector/loader project (`CatVapeExternal.exe` + `CatVapeBridge.dll`).

Build the external on Windows:

```bat
cd external
build.bat
```

The execution bridge is intentionally stubbed at `external/src/bridge/execute.cpp` - wire your Luau execution method there. See `external/README.md` for full details.

Regenerate artifacts after changing `src/`:

```bash
python3 scripts/build_injectable.py
python3 external/tools/embed_payload.py
```
