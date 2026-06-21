cmake_minimum_required(VERSION 3.10)
project(ccthread
    VERSION      1.0.0
    DESCRIPTION  "Cross-platform C/C++ concurrency primitives"
    LANGUAGES    C
)

# ====================================================================
#  Options
# ====================================================================
option(BUILD_EXAMPLES     "Build example programs"                      OFF)
option(BUILD_TESTING      "Register examples with CTest"                OFF)

# ====================================================================
#  Compiler defaults
# ====================================================================
set(CMAKE_C_STANDARD          99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS        OFF)

# --- Platform detection ---
if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    set(CCTHREAD_PLATFORM_MACOS TRUE)
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(CCTHREAD_PLATFORM_LINUX TRUE)
elseif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD|NetBSD|OpenBSD")
    set(CCTHREAD_PLATFORM_BSD TRUE)
elseif(CMAKE_SYSTEM_NAME MATCHES "AIX")
    set(CCTHREAD_PLATFORM_AIX TRUE)
elseif(CMAKE_SYSTEM_NAME MATCHES "SunOS")
    set(CCTHREAD_PLATFORM_SOLARIS TRUE)
elseif(CMAKE_SYSTEM_NAME MATCHES "HP-UX")
    set(CCTHREAD_PLATFORM_HPUX TRUE)
endif()

# --- Warnings ---
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -pedantic -Werror)
endif()

# --- Threading: pthread on all POSIX; Win32 API on Windows ---
if(UNIX)
    # -pthread compile flag (sets _REENTRANT, etc.)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
    # -lpthread link flag (macOS treats it as a no-op)
    set(CCTHREAD_PTHREAD_LIBS pthread)
endif()

# --- Static CRT on MSVC (required for static library) ---
if(MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MT")
endif()

# ====================================================================
#  Source files
# ====================================================================
set(CCTHREAD_SOURCES
    ccthread.c    ccthread.h
    ccsem.c       ccsem.h
    ccmutex.c     ccmutex.h
    ccspinlock.c
    ccrwlock.c
    cccondvar.c
    ccatomic.h
)

# ====================================================================
#  Object library (compile once, link twice — static + shared)
# ====================================================================
add_library(ccthread_obj OBJECT ${CCTHREAD_SOURCES})
target_include_directories(ccthread_obj PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties(ccthread_obj PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

if(UNIX)
    target_link_libraries(ccthread_obj PUBLIC ${CCTHREAD_PTHREAD_LIBS})
endif()

target_compile_definitions(ccthread_obj PRIVATE    CCTHREAD_BUILD_DLL)
if(CCTHREAD_PLATFORM_LINUX)
    target_compile_definitions(ccthread_obj PRIVATE _GNU_SOURCE)
endif()
target_compile_definitions(ccthread_obj INTERFACE  CCTHREAD_USE_DLL)

# ====================================================================
#  Shared library (always built)
# ====================================================================
add_library(ccthread SHARED $<TARGET_OBJECTS:ccthread_obj>)
target_include_directories(ccthread PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties(ccthread PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
if(UNIX)
    target_link_libraries(ccthread PUBLIC ${CCTHREAD_PTHREAD_LIBS})
endif()

# ====================================================================
#  Static library (always built, PIC for linking into shared libs)
# ====================================================================
add_library(ccthread_s STATIC $<TARGET_OBJECTS:ccthread_obj>)
target_include_directories(ccthread_s PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties(ccthread_s PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE
    OUTPUT_NAME ccthread
)
if(UNIX)
    target_link_libraries(ccthread_s PUBLIC ${CCTHREAD_PTHREAD_LIBS})
endif()

# ====================================================================
#  Static link helper target (non-macOS only)
# ====================================================================
if(UNIX AND NOT CCTHREAD_PLATFORM_MACOS)
    # Build a fully-static test binary to verify -static works
    add_executable(ccthread_static_test EXCLUDE_FROM_ALL
        examples/ccthread_basic.c)
    target_link_libraries(ccthread_static_test PRIVATE
        -static ccthread_s ${CCTHREAD_PTHREAD_LIBS})
    set_target_properties(ccthread_static_test PROPERTIES
        EXCLUDE_FROM_ALL TRUE
        EXCLUDE_FROM_DEFAULT_BUILD TRUE)
endif()

if(CCTHREAD_PLATFORM_MACOS)
    message(STATUS "ccthread: macOS detected — -static linking is not supported by the OS")
endif()

# ====================================================================
#  Examples
# ====================================================================
if(BUILD_EXAMPLES)
    set(EXAMPLE_LIBS ccthread)

    macro(add_ccthread_example name)
        add_executable(${name} examples/${name}.c)
        target_link_libraries(${name} PRIVATE ${EXAMPLE_LIBS})
    endmacro()

    add_ccthread_example(ccthread_basic)
    add_ccthread_example(ccthread_detach)
    add_ccthread_example(ccthread_naming)
    add_ccthread_example(ccsem_producer_consumer)
    add_ccthread_example(ccsem_timeout)
    add_ccthread_example(ccmutex_basic)
    add_ccthread_example(ccspinlock_basic)
    add_ccthread_example(ccrwlock_basic)
    add_ccthread_example(ccthread_once)
    add_ccthread_example(cccondvar_basic)
    add_ccthread_example(ccthread_lifecycle)

    if(BUILD_TESTING)
        enable_testing()
        add_test(NAME ccthread_basic      COMMAND ccthread_basic)
        add_test(NAME ccthread_detach     COMMAND ccthread_detach)
        add_test(NAME ccthread_naming     COMMAND ccthread_naming)
        add_test(NAME ccsem_producer_consumer COMMAND ccsem_producer_consumer)
        add_test(NAME ccsem_timeout       COMMAND ccsem_timeout)
        add_test(NAME ccmutex_basic       COMMAND ccmutex_basic)
        add_test(NAME ccspinlock_basic    COMMAND ccspinlock_basic)
        add_test(NAME ccrwlock_basic      COMMAND ccrwlock_basic)
        add_test(NAME ccthread_once       COMMAND ccthread_once)
        add_test(NAME cccondvar_basic     COMMAND cccondvar_basic)
        add_test(NAME ccthread_lifecycle  COMMAND ccthread_lifecycle)
    endif()
endif()

# ====================================================================
#  Installation
# ====================================================================
include(GNUInstallDirs)

install(TARGETS ccthread ccthread_s
    EXPORT  ccthread-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES
    ccthread.h
    ccsem.h
    ccmutex.h
    ccatomic.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
