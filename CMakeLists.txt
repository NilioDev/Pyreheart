cmake_minimum_required(VERSION 3.24)
project(pyreheart LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(FetchContent)

set(LIBTCOD_INSTALL OFF CACHE BOOL "" FORCE)
set(LIBTCOD_INSTALL_PKGCONFIG OFF CACHE BOOL "" FORCE)
set(LIBTCOD_THREADS OFF CACHE BOOL "" FORCE)
# libtcod's own copies: no patches to apply at configure time (they break on Windows line ends)
set(LIBTCOD_LODEPNG vendored CACHE STRING "" FORCE)
set(LIBTCOD_UTF8PROC vendored CACHE STRING "" FORCE)
set(LIBTCOD_STB vendored CACHE STRING "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
if(ANDROID)
    # Android loads SDL as its own library, next to ours ("main")
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
else()
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
endif()
FetchContent_Declare(libtcod
    GIT_REPOSITORY https://github.com/libtcod/libtcod
    GIT_TAG 2.2.2
    GIT_SHALLOW TRUE)
FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE)
set(JSON_Install OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(libtcod json)
if(MSVC)
    add_compile_options(/utf-8)
endif()

# the rules of the game: no window, no libtcod drawing; also used by the headless tests
add_library(pyrecore STATIC src/k5.cpp src/loot.cpp src/game.cpp src/view.cpp)
target_include_directories(pyrecore PUBLIC src)
target_link_libraries(pyrecore PUBLIC libtcod::libtcod nlohmann_json::nlohmann_json)

# the game itself
set(GAME_SOURCES src/main.cpp src/client.cpp src/logo.cpp src/fx.cpp src/net.cpp)
if(ANDROID)
    add_library(pyreheart SHARED ${GAME_SOURCES})
    set_target_properties(pyreheart PROPERTIES OUTPUT_NAME main)
    target_link_libraries(pyreheart PRIVATE pyrecore log android)
    return()
endif()
add_executable(pyreheart ${GAME_SOURCES})
target_link_libraries(pyreheart PRIVATE pyrecore)
if(WIN32)
    target_link_libraries(pyreheart PRIVATE ws2_32 iphlpapi)
    set_target_properties(pyreheart PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
add_custom_command(TARGET pyreheart POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:pyreheart>/assets)

enable_testing()
add_executable(k5_test tests/k5_test.cpp)
target_link_libraries(k5_test PRIVATE pyrecore)
add_test(NAME k5 COMMAND k5_test)
add_executable(bot tests/bot.cpp)
target_link_libraries(bot PRIVATE pyrecore)
add_test(NAME bot COMMAND bot)

add_executable(net_test tests/net_test.cpp src/net.cpp)
target_link_libraries(net_test PRIVATE pyrecore)
if(WIN32)
    target_link_libraries(net_test PRIVATE ws2_32 iphlpapi)
endif()
add_test(NAME net COMMAND net_test)
