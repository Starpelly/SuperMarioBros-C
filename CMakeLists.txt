cmake_minimum_required(VERSION 3.15)

set(PROJECT_NAME SuperMarioBrosC)

project(${PROJECT_NAME} LANGUAGES CXX)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_SUPPRESS_REGENERATION true)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/source)
set(VENDOR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor)

# Source files
set(SOURCES
    source/Configuration.cpp
    source/Emulation/APU.cpp
    source/Emulation/Controller.cpp
    source/Emulation/MemoryAccess.cpp
    source/Emulation/PPU.cpp
    source/Main.cpp
    source/OpenGLRendering.cpp
    source/SMB/SMB.cpp
    source/SMB/SMBData.cpp
    source/SMB/SMBEngine.cpp
    source/Util/Video.cpp
)

# Header files
set(HEADERS
    source/Configuration.hpp
    source/Constants.hpp
    source/Emulation/APU.hpp
    source/Emulation/Controller.hpp
    source/Emulation/MemoryAccess.hpp
    source/Emulation/PPU.hpp
    source/OpenGLRendering.hpp
    source/SMB/SMB.hpp
    source/SMB/SMBConstants.hpp
    source/SMB/SMBDataPointers.hpp
    source/SMB/SMBEngine.hpp
    source/Util/Video.hpp
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${SOURCE_DIR}
)

#====================================================================
# Platform independent dependencies
#====================================================================

#--------------------------------------------------------------------
# OpenGL
#--------------------------------------------------------------------
find_package(OpenGL REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE
    OpenGL::GL
)

#--------------------------------------------------------------------
# SDL2
#--------------------------------------------------------------------
set(SDL2_PATH ${VENDOR_DIR}/SDL2-2.32.10)
set(SDL2_DIR ${VENDOR_DIR}/SDL2-2.32.10/cmake)
find_package(SDL2 REQUIRED)

if (TARGET SDL2::SDL2-static)
    message(STATUS "Linking SDL2 statically")
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2-static)
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2main)
else()
    message(STATUS "Linking SDL32 dynamically")
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2)
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2main)

    # Copy SDL2.dll to the output dir, if we're linking dynamically
    if(WIN32)
        set(SDL2_DLL_PATH ${SDL2_PATH}/lib/x64/SDL2.dll)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SDL2_DLL_PATH}"
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
        )
    endif()
endif()

# Compiler warnings
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W3 /sdl)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
endif()

#====================================================================
# Folder structuring in Visual Studio
#====================================================================
if(MSVC_IDE)
	foreach(source IN LISTS SOURCES HEADERS)
		get_filename_component(source_path "${source}" PATH)
		string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" relative_source_path ${source_path})
		string(REPLACE "/" "\\" source_path_msvc ${relative_source_path})
		source_group(${source_path_msvc} FILES ${source})
    endforeach()
endif()