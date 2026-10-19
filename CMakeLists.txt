cmake_minimum_required(VERSION 3.20)
project(GtaVEnhancedPresence LANGUAGES CXX)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "GTA V Enhanced needs a 64-bit Windows build.")
endif()

set(SHV_SDK_DIR "" CACHE PATH "Script Hook V SDK directory containing inc/main.h and lib/ScriptHookV.lib")
if(NOT EXISTS "${SHV_SDK_DIR}/inc/main.h" OR
   NOT EXISTS "${SHV_SDK_DIR}/lib/ScriptHookV.lib")
    message(FATAL_ERROR "Set SHV_SDK_DIR to a Script Hook V SDK directory.")
endif()

set(PRESENCE_INI "${CMAKE_CURRENT_SOURCE_DIR}/GtaVEnhancedPresence.ini.example")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/GtaVEnhancedPresence.ini")
    set(PRESENCE_INI "${CMAKE_CURRENT_SOURCE_DIR}/GtaVEnhancedPresence.ini")
endif()

include(FetchContent)
FetchContent_Declare(discord_rpc
    URL https://github.com/discord/discord-rpc/archive/refs/tags/v3.4.0.zip)
FetchContent_Declare(rapidjson
    URL https://github.com/Tencent/rapidjson/archive/refs/tags/v1.1.0.zip)
FetchContent_GetProperties(discord_rpc)
if(NOT discord_rpc_POPULATED)
    FetchContent_Populate(discord_rpc)
endif()
FetchContent_GetProperties(rapidjson)
if(NOT rapidjson_POPULATED)
    FetchContent_Populate(rapidjson)
endif()

# Discord RPC's own CMake project is not used because it builds optional demo
# programs and platform variants that are unnecessary for this ASI.
add_library(presence_rpc STATIC
    ${discord_rpc_SOURCE_DIR}/src/discord_rpc.cpp
    ${discord_rpc_SOURCE_DIR}/src/rpc_connection.cpp
    ${discord_rpc_SOURCE_DIR}/src/serialization.cpp
    ${discord_rpc_SOURCE_DIR}/src/connection_win.cpp
    ${discord_rpc_SOURCE_DIR}/src/discord_register_win.cpp)
target_compile_features(presence_rpc PUBLIC cxx_std_14)
target_compile_definitions(presence_rpc PRIVATE DISCORD_WINDOWS)
target_include_directories(presence_rpc PUBLIC ${discord_rpc_SOURCE_DIR}/include
    PRIVATE ${rapidjson_SOURCE_DIR}/include)
target_link_libraries(presence_rpc PUBLIC psapi advapi32)

add_library(GtaVEnhancedPresence SHARED src/main.cpp)
target_compile_features(GtaVEnhancedPresence PRIVATE cxx_std_17)
target_compile_definitions(GtaVEnhancedPresence PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_include_directories(GtaVEnhancedPresence PRIVATE "${SHV_SDK_DIR}/inc")
target_link_libraries(GtaVEnhancedPresence PRIVATE
    "${SHV_SDK_DIR}/lib/ScriptHookV.lib" presence_rpc)
if(MINGW)
    target_link_options(GtaVEnhancedPresence PRIVATE -static)
endif()
set_target_properties(GtaVEnhancedPresence PROPERTIES PREFIX "" SUFFIX ".asi")

add_custom_command(TARGET GtaVEnhancedPresence POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PRESENCE_INI}" "$<TARGET_FILE_DIR:GtaVEnhancedPresence>/GtaVEnhancedPresence.ini"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/GtaVEnhancedPresence.missions.ini"
        "$<TARGET_FILE_DIR:GtaVEnhancedPresence>/GtaVEnhancedPresence.missions.ini")
