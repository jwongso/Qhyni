cmake_minimum_required(VERSION 3.14)
project(Qhyni LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
cmake_policy(SET CMP0167 OLD)

option(ENABLE_AUDIO_STREAM "Enable audio streaming feature" OFF)

find_package(Boost REQUIRED COMPONENTS system)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets WebSockets)

if(ENABLE_AUDIO_STREAM)
    find_package(Qt6 REQUIRED COMPONENTS Multimedia)
endif()

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
        "$<$<AND:$<CONFIG:RELEASE>,$<COMPILE_LANGUAGE:CXX>>:-MMD;-Wall;-Wno-deprecated-declarations;-march=alderlake;-O3;-march=native;-std=c++20;-fPIC;-DQT_NO_DEBUG_OUTPUT;-flto;-fomit-frame-pointer;-DNDEBUG>"
        "$<$<AND:$<CONFIG:DEBUG>,$<COMPILE_LANGUAGE:CXX>>:-MMD;-Wall;-Wno-deprecated-declarations;-O0;-g3;-std=c++20;-D_GLIBCXX_DEBUG;-fPIC;>"
        "$<$<AND:$<CONFIG:RELWITHDEBINFO>,$<COMPILE_LANGUAGE:CXX>>:-MMD;-Wall;-Wno-deprecated-declarations;-march=alderlake;-O3;-g;-std=c++20;-fPIC;-DQT_NO_DEBUG_OUTPUT;-flto;-fomit-frame-pointer;>"
        "$<$<AND:$<CONFIG:RELEASE>,$<COMPILE_LANGUAGE:C>>:-MMD;-Wall;-Wno-deprecated-declarations;-march=alderlake;-O3;-fPIC;-DQT_NO_DEBUG_OUTPUT;-flto;-fomit-frame-pointer;-DNDEBUG>"
        "$<$<AND:$<CONFIG:DEBUG>,$<COMPILE_LANGUAGE:C>>:-MMD;-Wall;-Wno-deprecated-declarations;-O0;-g3;-D_GLIBCXX_DEBUG;-fPIC;>"
        "$<$<AND:$<CONFIG:RELWITHDEBINFO>,$<COMPILE_LANGUAGE:C>>:-MMD;-Wall;-Wno-deprecated-declarations;-march=alderlake;-O3;-g;-fPIC;-DQT_NO_DEBUG_OUTPUT;-flto;-fomit-frame-pointer;>"
)
add_compile_options("${opts}")

include(FetchContent)

set(HYNI_REPO "git@github.com:jwongso/hyni.git")
set(HYNI_TAG "main")  # Can be a branch name, tag, or commit hash

# Declare the FetchContent dependency
FetchContent_Declare(
    hyni
    GIT_REPOSITORY ${HYNI_REPO}
    GIT_TAG ${HYNI_TAG}
    GIT_PROGRESS TRUE
)

FetchContent_GetProperties(hyni)

if(NOT hyni_POPULATED)
    message(STATUS "Fetching hyni for the first time...")
else()
    message(STATUS "Checking for updates in hyni...")

    # Run git fetch to check for new commits
    execute_process(
        COMMAND git fetch
        WORKING_DIRECTORY ${hyni_SOURCE_DIR}
        RESULT_VARIABLE GIT_FETCH_RESULT
        ERROR_QUIET
    )

    # Get the latest remote commit hash
    execute_process(
        COMMAND git rev-parse origin/${HYNI_TAG}
        WORKING_DIRECTORY ${hyni_SOURCE_DIR}
        OUTPUT_VARIABLE REMOTE_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    # Get the currently checked-out commit hash
    execute_process(
        COMMAND git rev-parse HEAD
        WORKING_DIRECTORY ${hyni_SOURCE_DIR}
        OUTPUT_VARIABLE LOCAL_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if(NOT "${REMOTE_COMMIT}" STREQUAL "${LOCAL_COMMIT}")
        message(STATUS "New commit detected in hyni, pulling latest changes...")

        execute_process(
            COMMAND git pull
            WORKING_DIRECTORY ${hyni_SOURCE_DIR}
            RESULT_VARIABLE GIT_PULL_RESULT
            ERROR_QUIET
        )

        if(GIT_PULL_RESULT EQUAL 0)
            message(STATUS "hyni updated successfully! Reconfiguring project...")
            set_property(DIRECTORY PROPERTY CMAKE_CONFIGURE_DEPENDS "${hyni_SOURCE_DIR}")
        else()
            message(WARNING "hyni update failed or no new changes were found.")
        endif()
    else()
        message(STATUS "hyni is already up to date.")
    endif()
endif()

# Make hyni available
FetchContent_MakeAvailable(hyni)

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
    src/PngMonitor.h
)

if(ENABLE_AUDIO_STREAM)
    set(UI_HEADERS ${UI_HEADERS} src/AudioStreamer.h)
    set(UI_SOURCES ${UI_SOURCES} src/AudioStreamer.cpp)
endif()

qt_add_executable(${PROJECT_NAME} ${UI_SOURCES} ${UI_HEADERS})

if(ENABLE_AUDIO_STREAM)
    target_link_libraries(${PROJECT_NAME} PRIVATE hyni Qt6::Core Qt6::Gui Qt6::Widgets Qt6::WebSockets Qt6::Multimedia)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_AUDIO_STREAM)
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE hyni Qt6::Core Qt6::Gui Qt6::Widgets Qt6::WebSockets)
endif()

if(TARGET hyni)
    # Ensure headers from hyni are included
    get_target_property(HYNI_INCLUDE_DIRS hyni INTERFACE_INCLUDE_DIRECTORIES)
    if(HYNI_INCLUDE_DIRS)
        target_include_directories(${PROJECT_NAME} PRIVATE ${HYNI_INCLUDE_DIRS})
    else()
        # Fallback: Manually add expected include paths
        target_include_directories(${PROJECT_NAME} PRIVATE
            ${hyni_SOURCE_DIR}/include
            ${hyni_SOURCE_DIR}/src
            ${hyni_BINARY_DIR}  # For generated headers, if any
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
