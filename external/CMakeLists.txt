cmake_minimum_required(VERSION 3.20)
project(CatVapeExternal VERSION 1.0.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# --- Bridge DLL (injected into Roblox) ---
add_library(CatVapeBridge SHARED
    src/bridge/dllmain.cpp
    src/bridge/bridge.cpp
    src/bridge/pipe.cpp
    src/bridge/execute.cpp
)

target_include_directories(CatVapeBridge PRIVATE src/bridge)
target_compile_definitions(CatVapeBridge PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)

if(MSVC)
    set_target_properties(CatVapeBridge PROPERTIES OUTPUT_NAME "CatVapeBridge")
else()
    set_target_properties(CatVapeBridge PROPERTIES OUTPUT_NAME "CatVapeBridge" PREFIX "" SUFFIX ".dll")
    target_link_options(CatVapeBridge PRIVATE -static -static-libgcc -static-libstdc++)
endif()

target_link_libraries(CatVapeBridge PRIVATE user32 kernel32)

# --- Injector EXE ---
add_executable(CatVapeInjector WIN32
    src/injector/main.cpp
    src/injector/process.cpp
    src/injector/injector.cpp
    src/injector/pipe_client.cpp
    src/injector/payload_loader.cpp
    src/injector/payload.rc
)

target_include_directories(CatVapeInjector PRIVATE src/injector)
target_compile_definitions(CatVapeInjector PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)

if(MSVC)
    set_target_properties(CatVapeInjector PROPERTIES OUTPUT_NAME "CatVapeExternal")
else()
    set_target_properties(CatVapeInjector PROPERTIES OUTPUT_NAME "CatVapeExternal")
    target_link_options(CatVapeInjector PRIVATE -municode -static -static-libgcc -static-libstdc++)
endif()

target_link_libraries(CatVapeInjector PRIVATE user32 kernel32 advapi32 gdi32 msimg32 gdiplus)

# Put outputs in a predictable place
set_target_properties(CatVapeBridge CatVapeInjector PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
