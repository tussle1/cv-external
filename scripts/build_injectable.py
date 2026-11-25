#!/usr/bin/env python3
"""
Build a single-file, executor-injectable CatVape loader.

Embeds every file under src/ as base64 into a Lua script that writes them
into the executor workspace (catsix/) and boots the cheat. No external
downloads, no workspace folder setup - just inject and run.

Usage:
    python3 scripts/build_injectable.py
Output:
    CatVape-Injectable.lua (repo root)
"""
from __future__ import annotations

import base64
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src"
OUT = ROOT / "CatVape-Injectable.lua"

# Files under src/ that should be embedded. Everything in src/ is included by
# default; this keeps the build deterministic and easy to audit.
EXCLUDE_SUFFIXES = {".DS_Store"}
EXCLUDE_NAMES = {".DS_Store"}


def collect_files() -> list[tuple[str, bytes]]:
    files: list[tuple[str, bytes]] = []
    for path in sorted(SRC.rglob("*")):
        if not path.is_file():
            continue
        if path.name in EXCLUDE_NAMES or path.suffix in EXCLUDE_SUFFIXES:
            continue
        rel = path.relative_to(SRC).as_posix()
        files.append((rel, path.read_bytes()))
    return files


def lua_string(s: str) -> str:
    """Encode a python string as a Lua long string literal."""
    # Base64 never contains ']]', but be defensive.
    eq = 0
    marker = "]" + ("=" * eq) + "]"
    while marker in s:
        eq += 1
        marker = "]" + ("=" * eq) + "]"
    open_m = "[" + ("=" * eq) + "["
    close_m = "]" + ("=" * eq) + "]"
    return open_m + s + close_m


BOOT = r'''--!nocheck
--[[
    CatVape External - single-file injectable build
    Generated from src/ by scripts/build_injectable.py

    What this does:
      1. Embeds every src/ file as base64 (no external downloads).
      2. Writes them into the executor workspace under catsix/.
      3. Boots the GUI, universal/game modules, main library, and deferred extras.

    Usage:
      Paste this whole file into your executor and execute it.
      Requires standard executor filesystem APIs (readfile/writefile/isfile/
      isfolder/makefolder/listfiles) and loadstring/getcustomasset.
]]

local Directory = "catsix"

-- Base64 decode. Prefers the executor's fast built-in when it is safe to use,
-- falls back to a pure-Lua decoder for executors where base64decode returns
-- garbage or a userdata object.
local DecodeVerified = false
local ExecutorDecode = nil
do
    local Fn = (crypt and (crypt.base64decode or crypt.base64_decode))
        or base64_decode or base64decode
    if type(Fn) == "function" then
        local Ok, Result = pcall(Fn, "dGVzdA==")
        if Ok and Result == "test" then
            ExecutorDecode = Fn
            DecodeVerified = true
        end
    end
end

local B64 = {}
do
    local Alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
    for I = 1, #Alphabet do
        B64[string.byte(Alphabet, I)] = I - 1
    end
    B64[string.byte("=")] = -1
end

local function PureDecode(Input: string): string
    local Bytes = table.create(math.floor(#Input / 4) * 3)
    local BitBuffer, BitCount, OutIndex = 0, 0, 0
    for I = 1, #Input do
        local Value = B64[string.byte(Input, I)]
        if Value then
            if Value >= 0 then
                BitBuffer = bit32.bor(bit32.lshift(BitBuffer, 6), Value)
                BitCount = BitCount + 6
                if BitCount >= 8 then
                    BitCount = BitCount - 8
                    OutIndex = OutIndex + 1
                    Bytes[OutIndex] = string.char(bit32.band(bit32.rshift(BitBuffer, BitCount), 255))
                end
            end
        end
    end
    return table.concat(Bytes)
end

local function Decode(Input: string): string
    if DecodeVerified and ExecutorDecode then
        local Ok, Result = pcall(ExecutorDecode, Input)
        if Ok and type(Result) == "string" then
            return Result
        end
    end
    return PureDecode(Input)
end

if not game:IsLoaded() then
    game.Loaded:Wait()
end
if shared.vapeloading and coroutine.status(shared.vapeloading) ~= "dead" then
    return
end
shared.vapeloading = coroutine.running()
if shared.vape then
    pcall(function() shared.vape:Uninject() end)
end

for _, Folder in {
    Directory,
    Directory .. "/games",
    Directory .. "/profiles",
    Directory .. "/assets",
    Directory .. "/assets/new",
    Directory .. "/libraries",
    Directory .. "/guis",
} do
    if not isfolder(Folder) then
        makefolder(Folder)
    end
end

local Installed, Skipped = 0, 0
for Rel, B64Data in Embedded do
    local Dest = Directory .. "/" .. Rel
    local Parent = Dest:match("^(.+)/[^/]+$")
    if Parent and not isfolder(Parent) then
        makefolder(Parent)
    end

    local Ok, Content = pcall(Decode, B64Data)
    if Ok and type(Content) == "string" and #Content > 0 then
        -- Never overwrite user profiles/configs on re-inject.
        local IsProfile = Dest:find("/profiles/", 1, true) ~= nil
        local ShouldWrite = true
        if IsProfile and isfile(Dest) then
            ShouldWrite = false
        elseif isfile(Dest) then
            local OkRead, Existing = pcall(readfile, Dest)
            ShouldWrite = (not OkRead) or Existing ~= Content
        end

        if ShouldWrite then
            local WOk = pcall(writefile, Dest, Content)
            if WOk then
                Installed = Installed + 1
            else
                Skipped = Skipped + 1
            end
        end
    else
        Skipped = Skipped + 1
        warn("[catvape-external] failed to decode: " .. tostring(Rel))
    end
end
print(string.format("[catvape-external] embedded files ready: %d written, %d skipped", Installed, Skipped))

getgenv().catuser = "Slop"
getgenv().catrole = "premium"

local isfile = isfile or function(FilePath)
    local Success, Contents = pcall(function()
        return readfile(FilePath)
    end)
    return Success and Contents ~= nil and Contents ~= ""
end
local cloneref = cloneref or function(Reference) return Reference end
local Players = cloneref(game:GetService("Players"))
local HttpService = cloneref(game:GetService("HttpService"))

local License = { Key = "slop", Callback = function() end }

local vape, RunPremium
local ls = function(...)
    local Chunk, Error = loadstring(...)
    if Error then
        warn("[catvape-external] compile error: " .. tostring(Error))
    end
    if Error and vape then
        pcall(function()
            vape:CreateNotification("Vape", "Failed to load : " .. tostring(Error), 30, "alert")
        end)
    end
    return Chunk
end

if not isfile(Directory .. "/profiles/gui.txt") then
    writefile(Directory .. "/profiles/gui.txt", "new")
end
if not isfolder(Directory .. "/assets/new") then
    makefolder(Directory .. "/assets/new")
end

local GuiChunk = ls(readfile(Directory .. "/guis/new.lua"), "gui")
if not GuiChunk then
    error("[catvape-external] gui failed to compile - re-inject the loader")
end
vape, RunPremium = GuiChunk(License)
shared.vape = vape
_G.vape = vape
getgenv().used_init = true

local function FinishLoading()
    vape.Init = nil
    vape:Load()
    shared.vapeloading = nil

    local queue_on_teleport = queue_on_teleport or function() end
    local TeleportedServers
    vape:Clean(Players.LocalPlayer.OnTeleport:Connect(function()
        if not TeleportedServers then
            TeleportedServers = true
            -- Re-inject from the local file written by this build if present,
            -- otherwise the user can re-execute the same injectable.
            local TeleportScript = [[
                shared.vapereload = true
                if isfile and isfile("CatVape-Injectable.lua") then
                    loadstring(readfile("CatVape-Injectable.lua"), "CatVape-External")()
                elseif isfile and isfile("catsix/init.lua") then
                    loadstring(readfile("catsix/init.lua"), "catvape-init")()
                end
            ]]
            pcall(function() vape:Save() end)
            pcall(queue_on_teleport, TeleportScript)
        end
    end))

    pcall(function()
        vape:CreateNotification("CatVape External", "Loaded. Authenticated as Slop with premium.", 5)
    end)
end

-- Core boot: universal, place-specific game module, main library.
local function SafeLoad(Rel, Name, ...)
    if not isfile(Directory .. "/" .. Rel) then
        warn("[catvape-external] missing: " .. Rel)
        return nil
    end
    local Chunk = ls(readfile(Directory .. "/" .. Rel), Name)
    if not Chunk then
        warn("[catvape-external] compile failed: " .. Rel)
        return nil
    end
    local Ok, Result = pcall(Chunk, ...)
    if not Ok then
        warn("[catvape-external] runtime error in " .. Rel .. ": " .. tostring(Result))
    end
    return Ok, Result
end

SafeLoad("games/universal.lua", "universal", License)
local PlaceFile = "games/" .. tostring(game.PlaceId) .. ".lua"
if isfile(Directory .. "/" .. PlaceFile) then
    SafeLoad(PlaceFile, tostring(game.PlaceId), License)
end
SafeLoad("libraries/main.lua", "main", License)

-- Deferred modules: load after GUI is up so boot stays responsive.
task.spawn(function()
    local Deferred = {
        "games/extras.lua",
        "games/staffdetector.lua",
        "games/spectatordetector.lua",
        "games/cheatwatcher.lua",
        "games/ghostfix.lua",
        "games/aar.lua",
        "games/playui.lua",
        "games/syle.lua",
        "games/aeroports.lua",
        "games/projectileleadsync.lua",
        "games/voteviewer.lua",
        "games/fighttriage.lua",
    }

    for _, Rel in Deferred do
        local Dest = Directory .. "/" .. Rel
        local Waited = 0
        while not isfile(Dest) and Waited < 5 do
            task.wait(0.25)
            Waited = Waited + 0.25
        end
    end

    task.wait(0.5)
    for _, Rel in Deferred do
        if isfile(Directory .. "/" .. Rel) then
            local Name = Rel:match("([^/]+)%.lua$") or Rel
            local Chunk = ls(readfile(Directory .. "/" .. Rel), Name)
            if Chunk then
                local Ok, Err = pcall(Chunk)
                if not Ok then
                    warn("[catvape-external] deferred " .. Name .. " error: " .. tostring(Err))
                end
            end
            task.wait(0.15)
        end
    end

    -- Re-apply saved profile state for modules that registered after vape:Load.
    task.delay(2, function()
        pcall(function()
            if not vape or not vape.Loaded then return end
            local ProfilePath = Directory .. "/profiles/" .. tostring(vape.Profile or "default") .. tostring(vape.Place or "") .. ".txt"
            if not isfile(ProfilePath) then return end
            local Data = HttpService:JSONDecode(readfile(ProfilePath))
            if type(Data) ~= "table" or type(Data.Modules) ~= "table" then return end
            local Applied = 0
            local I = 0
            for _, Module in vape.Modules do
                local Saved = Data.Modules[Module.Name]
                if type(Saved) == "table" and type(Module.Load) == "function" then
                    pcall(function() Module:Load(Saved) end)
                    Applied = Applied + 1
                    I = I + 1
                    if I % 8 == 0 then task.wait() end
                end
            end
            print("[catvape-external] restored saved state for " .. Applied .. " deferred modules")
        end)
    end)
end)

FinishLoading()
'''


def build() -> None:
    files = collect_files()
    if not files:
        raise SystemExit(f"no files found under {SRC}")

    lines: list[str] = []
    lines.append(BOOT.rstrip())
    lines.append("")
    lines.append("-- Embedded src/ payload: [relative_path] = base64(content)")
    lines.append("local Embedded = {")

    total_raw = 0
    for rel, data in files:
        total_raw += len(data)
        b64 = base64.b64encode(data).decode("ascii")
        # Lua table key as string literal
        key = rel.replace("\\", "\\\\").replace('"', '\\"')
        lines.append(f'    ["{key}"] = {lua_string(b64)},')

    lines.append("}")
    lines.append("")
    # The BOOT references `Embedded` as a global/local above? Wait - in the
    # boot code I wrote `for Rel, B64Data in Embedded do` but `Embedded` is
    # defined AFTER boot in this layout. That's a problem. I need to define
    # Embedded before the boot uses it.
    # Fix: restructure - emit Embedded first, then boot. But boot is a big
    # string constant. Easier to emit in correct order here.
    # I'll handle it by building in order: header+Embedded, then boot logic.

    # Rebuild properly
    out_lines: list[str] = []
    # Split BOOT into header (up to before "local Installed, Skipped") and rest?
    # Easier to just restructure the BOOT string to put a placeholder.
    # But I already wrote BOOT as a raw string. I'll split at the marker.
    marker = "local Installed, Skipped = 0, 0"
    if marker not in BOOT:
        raise SystemExit("boot marker not found")
    before, after = BOOT.split(marker, 1)

    out_lines.append(before.rstrip())
    out_lines.append("")
    out_lines.append("-- Embedded src/ payload: [relative_path] = base64(content)")
    out_lines.append("local Embedded = {")
    for rel, data in files:
        b64 = base64.b64encode(data).decode("ascii")
        key = rel.replace("\\", "\\\\").replace('"', '\\"')
        out_lines.append(f'    ["{key}"] = {lua_string(b64)},')
    out_lines.append("}")
    out_lines.append("")
    out_lines.append(marker + after.rstrip())
    out_lines.append("")

    OUT.write_text("\n".join(out_lines), encoding="utf-8")
    size = OUT.stat().st_size
    print(f"wrote {OUT} ({size/1024/1024:.2f} MB) with {len(files)} files, {total_raw/1024/1024:.2f} MB raw")


if __name__ == "__main__":
    build()
