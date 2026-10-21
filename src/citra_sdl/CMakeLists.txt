set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/CMakeModules)

if (EMSCRIPTEN)
    add_executable(citra_sdl)
else()
    add_library(citra_sdl STATIC EXCLUDE_FROM_ALL)
endif()
target_sources(citra_sdl PRIVATE
    config.cpp
    config.h
    default_ini.h
    emu_window/emu_window_sdl2.cpp
    emu_window/emu_window_sdl2.h
    citra_sdl.cpp
    precompiled_headers.h
    resource.h
)

if (ENABLE_SOFTWARE_RENDERER)
    target_sources(citra_sdl PRIVATE
        emu_window/emu_window_sdl2_sw.cpp
        emu_window/emu_window_sdl2_sw.h
    )
endif()
if (ENABLE_OPENGL)
    target_sources(citra_sdl PRIVATE
        emu_window/emu_window_sdl2_gl.cpp
        emu_window/emu_window_sdl2_gl.h
    )
endif()
if (ENABLE_VULKAN)
    target_sources(citra_sdl PRIVATE
        emu_window/emu_window_sdl2_vk.cpp
        emu_window/emu_window_sdl2_vk.h
    )
endif()

create_target_directory_groups(citra_sdl)

target_link_libraries(citra_sdl PRIVATE citra_common citra_core input_common network)
target_link_libraries(citra_sdl PRIVATE inih)
if (MSVC)
    target_link_libraries(citra_sdl PRIVATE getopt)
endif()
target_link_libraries(citra_sdl PRIVATE ${PLATFORM_LIBRARIES} SDL2::SDL2 Threads::Threads)

if (ENABLE_OPENGL)
    target_link_libraries(citra_sdl PRIVATE glad)
endif()

if (CITRA_USE_PRECOMPILED_HEADERS)
    target_precompile_headers(citra_sdl PRIVATE precompiled_headers.h)
endif()

if (EMSCRIPTEN)
    set_target_properties(citra_sdl PROPERTIES OUTPUT_NAME "azahar" SUFFIX ".js")
    target_link_options(citra_sdl PRIVATE
        -sUSE_PTHREADS=1 -sPTHREAD_POOL_SIZE=16
        -sINITIAL_MEMORY=805306368 -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=4294967296
        -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sGL_ENABLE_GET_PROC_ADDRESS=1
        -sFULL_ES3=1 -sSTACK_SIZE=8388608 -sDEFAULT_PTHREAD_STACK_SIZE=4194304
        -sEXIT_RUNTIME=0 -sINVOKE_RUN=0 -sMODULARIZE=0
        -sEXPORTED_RUNTIME_METHODS=callMain,FS,ENV
        -sFORCE_FILESYSTEM=1 -lidbfs.js
        -sASSERTIONS=1)
endif()
