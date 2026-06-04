cmake_minimum_required(VERSION 3.10)
project(ccthread
    VERSION      1.0.0
    DESCRIPTION  "Cross-platform C/C++ concurrency primitives"
    LANGUAGES    C
)

# --------------------------------------------------------------------
#  Options
# --------------------------------------------------------------------
option(BUILD_SHARED_LIBS  "Build shared libraries (.dll / .so / .dylib)" OFF)
option(BUILD_EXAMPLES     "Build example programs"                      ON)

# --------------------------------------------------------------------
#  Compiler defaults
# --------------------------------------------------------------------
set(CMAKE_C_STANDARD          99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS        OFF)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# --------------------------------------------------------------------
#  ccthread  — thread library
# --------------------------------------------------------------------
add_library(ccthread
    ccthread.c
    ccthread.h
)

target_include_directories(ccthread PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

# Link pthread on POSIX
if(UNIX)
    target_link_libraries(ccthread PUBLIC pthread)
endif()

# DLL export / import annotations
target_compile_definitions(ccthread PRIVATE
    $<$<BOOL:${BUILD_SHARED_LIBS}>:CCTHREAD_BUILD_DLL>
)
target_compile_definitions(ccthread INTERFACE
    $<$<BOOL:${BUILD_SHARED_LIBS}>:CCTHREAD_USE_DLL>
)

# --------------------------------------------------------------------
#  ccsem  — semaphore library
# --------------------------------------------------------------------
add_library(ccsem
    ccsem.c
    ccsem.h
)

target_include_directories(ccsem PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

# pthread needed on Linux / BSD (macOS uses GCD which is in libSystem)
if(UNIX AND NOT APPLE)
    target_link_libraries(ccsem PUBLIC pthread)
endif()

target_compile_definitions(ccsem PRIVATE
    $<$<BOOL:${BUILD_SHARED_LIBS}>:CCSEM_BUILD_DLL>
)
target_compile_definitions(ccsem INTERFACE
    $<$<BOOL:${BUILD_SHARED_LIBS}>:CCSEM_USE_DLL>
)

# --------------------------------------------------------------------
#  Examples
# --------------------------------------------------------------------
if(BUILD_EXAMPLES)

    # ---- ccthread examples ----

    add_executable(ccthread_basic  examples/ccthread_basic.c)
    target_link_libraries(ccthread_basic PRIVATE ccthread)

    add_executable(ccthread_detach examples/ccthread_detach.c)
    target_link_libraries(ccthread_detach PRIVATE ccthread)

    add_executable(ccthread_naming examples/ccthread_naming.c)
    target_link_libraries(ccthread_naming PRIVATE ccthread)

    # ---- ccsem examples ----

    add_executable(ccsem_producer_consumer
        examples/ccsem_producer_consumer.c
    )
    target_link_libraries(ccsem_producer_consumer PRIVATE ccsem ccthread)

    add_executable(ccsem_timeout
        examples/ccsem_timeout.c
    )
    target_link_libraries(ccsem_timeout PRIVATE ccsem ccthread)

endif()

# --------------------------------------------------------------------
#  Installation
# --------------------------------------------------------------------
include(GNUInstallDirs)

install(TARGETS ccthread ccsem
    EXPORT  ccthread-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES
    ccthread.h
    ccsem.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
