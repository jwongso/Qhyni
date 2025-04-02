cmake_minimum_required(VERSION 3.14)
project(Qhyni LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
cmake_policy(SET CMP0167 OLD)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets WebSockets)

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    message("Found ccache ${CCACHE_FOUND}")
    message("Using ccache to speed up compilation")
    set(ENV{CCACHE_CPP2} "yes")
    set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_FOUND})
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_FOUND})
endif(CCACHE_FOUND)

string(
    APPEND opts
        "$<$<AND:$<CONFIG:RELEASE>,$<COMPILE_LANGUAGE:CXX>>:-MMD;-Wall;-Wno-deprecated-declarations;-march=alderlake;-O3;-march=native;-std=c++20;-fPIC;-DBOOST_BIND_GLOBAL_PLACEHOLDERS;-DQT_NO_DEBUG_OUTPUT;-flto;-fomit-frame-pointer;-DNDEBUG>"
        "$<$<AND:$<CONFIG:DEBUG>,$<COMPILE_LANGUAGE:CXX>>:-MMD;-Wall;-Wno-deprecated-declarations;-O0;-g3;-std=c++20;-D_GLIBCXX_DEBUG;-fPIC;-DBOOST_BIND_GLOBAL_PLACEHOLDERS>"
        "$<$<AND:$<CONFIG:RELWITHDEBINFO>,$<COMPILE_LANGUAGE:CXX>>:-MMD;-Wall;-Wno-deprecated-declarations;-march=alderlake;-O3;-g;-std=c++20;-fPIC;-DBOOST_BIND_GLOBAL_PLACEHOLDERS;-DQT_NO_DEBUG_OUTPUT;-flto;-fomit-frame-pointer;-DNDEBUG>"
        "$<$<AND:$<CONFIG:RELEASE>,$<COMPILE_LANGUAGE:C>>:-MMD;-Wall;-Wno-deprecated-declarations;-march=alderlake;-O3;-fPIC;-DBOOST_BIND_GLOBAL_PLACEHOLDERS;-DQT_NO_DEBUG_OUTPUT;-flto;-fomit-frame-pointer;-DNDEBUG>"
        "$<$<AND:$<CONFIG:DEBUG>,$<COMPILE_LANGUAGE:C>>:-MMD;-Wall;-Wno-deprecated-declarations;-O0;-g3;-D_GLIBCXX_DEBUG;-fPIC;-DBOOST_BIND_GLOBAL_PLACEHOLDERS>"
        "$<$<AND:$<CONFIG:RELWITHDEBINFO>,$<COMPILE_LANGUAGE:C>>:-MMD;-Wall;-Wno-deprecated-declarations;-march=alderlake;-O3;-g;-fPIC;-DBOOST_BIND_GLOBAL_PLACEHOLDERS;-DQT_NO_DEBUG_OUTPUT;-flto;-fomit-frame-pointer;-DNDEBUG>"
)
add_compile_options("${opts}")

include(FetchContent)

set(FETCHCONTENT_QUIET OFF)
# Fetch the hyni project via SSH
FetchContent_Declare(
    hyni
    GIT_REPOSITORY git@github.com:jwongso/hyni.git
    GIT_TAG main
    GIT_SHALLOW TRUE
)

FetchContent_GetProperties(hyni)
add_subdirectory(${hyni_SOURCE_DIR} ${hyni_BINARY_DIR})

# Find Boost (if needed by hyni)
find_package(Boost REQUIRED COMPONENTS system)

# Your UI sources
set(UI_SOURCES
    src/HyniWindow.cpp
    src/HighlightTableWidget.cpp
    src/ChatAPIWorker.cpp
    src/main.cpp
)

set(UI_HEADERS
    src/HyniWindow.h
    src/HighlightTableWidget.h
    src/ChatAPIWorker.h
)

qt_add_executable(${PROJECT_NAME} ${UI_SOURCES} ${UI_HEADERS})

target_link_libraries(${PROJECT_NAME} PRIVATE hyni Qt6::Core Qt6::Gui Qt6::Widgets Qt6::WebSockets)

if(TARGET hyni)
    target_link_libraries(${PROJECT_NAME} PRIVATE hyni)

    # Modern CMake projects should automatically propagate their include directories
    # through their target properties, but if they don't:
    get_target_property(HYNI_INCLUDE_DIRS hyni INTERFACE_INCLUDE_DIRECTORIES)
    if(HYNI_INCLUDE_DIRS)
        target_include_directories(${PROJECT_NAME} PRIVATE ${HYNI_INCLUDE_DIRS})
    else()
        # Fallback: Manually add expected include paths
        target_include_directories(${PROJECT_NAME} PRIVATE
            ${hyni_SOURCE_DIR}/include
            ${hyni_SOURCE_DIR}/src
            ${hyni_BINARY_DIR}  # For generated headers if any
        )
    endif()
else()
    message(WARNING "hyni target not found - trying manual includes")
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${hyni_SOURCE_DIR}/include
        ${hyni_SOURCE_DIR}/src
    )
endif()

set_property(TARGET ${PROJECT_NAME} PROPERTY AUTOMOC ON)

# Set Qt GUI as the main application type
set_target_properties(${PROJECT_NAME} PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)
