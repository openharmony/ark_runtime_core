cmake_minimum_required(VERSION 2.4.4)
set(CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS ON)

project(zlib C)

set(VERSION "1.2.11")

option(ASM686 "Enable building i686 assembly implementation")
option(AMD64 "Enable building amd64 assembly implementation")

set(INSTALL_BIN_DIR "${CMAKE_INSTALL_PREFIX}/bin" CACHE PATH "Installation directory for executables")
set(INSTALL_LIB_DIR "${CMAKE_INSTALL_PREFIX}/lib" CACHE PATH "Installation directory for libraries")
set(INSTALL_INC_DIR "${CMAKE_INSTALL_PREFIX}/include" CACHE PATH "Installation directory for headers")
set(INSTALL_MAN_DIR "${CMAKE_INSTALL_PREFIX}/share/man" CACHE PATH "Installation directory for manual pages")
set(INSTALL_PKGCONFIG_DIR "${CMAKE_INSTALL_PREFIX}/share/pkgconfig" CACHE PATH "Installation directory for pkgconfig (.pc) files")

include(CheckTypeSize)
include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckCSourceCompiles)
enable_testing()

check_include_file(sys/types.h HAVE_SYS_TYPES_H)
check_include_file(stdint.h    HAVE_STDINT_H)
check_include_file(stddef.h    HAVE_STDDEF_H)

#
# Check to see if we have large file support
#
set(CMAKE_REQUIRED_DEFINITIONS -D_LARGEFILE64_SOURCE=1)
# We add these other definitions here because CheckTypeSize.cmake
# in CMake 2.4.x does not automatically do so and we want
# compatibility with CMake 2.4.x.
if(HAVE_SYS_TYPES_H)
    list(APPEND CMAKE_REQUIRED_DEFINITIONS -DHAVE_SYS_TYPES_H)
endif()
if(HAVE_STDINT_H)
    list(APPEND CMAKE_REQUIRED_DEFINITIONS -DHAVE_STDINT_H)
endif()
if(HAVE_STDDEF_H)
    list(APPEND CMAKE_REQUIRED_DEFINITIONS -DHAVE_STDDEF_H)
endif()
check_type_size(off64_t OFF64_T)
if(HAVE_OFF64_T)
   add_definitions(-D_LARGEFILE64_SOURCE=1)
endif()
set(CMAKE_REQUIRED_DEFINITIONS) # clear variable

#
# Check for fseeko
#
check_function_exists(fseeko HAVE_FSEEKO)
if(NOT HAVE_FSEEKO)
    add_definitions(-DNO_FSEEKO)
endif()

#
# Check for unistd.h
#
check_include_file(unistd.h Z_HAVE_UNISTD_H)

if(MSVC)
    set(CMAKE_DEBUG_POSTFIX "d")
    add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
    add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR})
endif()

if(NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_CURRENT_BINARY_DIR)
    # If we're doing an out of source build and the user has a zconf.h
    # in their source tree...
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/zconf.h)
        message(STATUS "Renaming")
        message(STATUS "    ${CMAKE_CURRENT_SOURCE_DIR}/zconf.h")
        message(STATUS "to 'zconf.h.included' because this file is included with zlib")
        message(STATUS "but CMake generates it automatically in the build directory.")
        file(RENAME ${CMAKE_CURRENT_SOURCE_DIR}/zconf.h ${CMAKE_CURRENT_SOURCE_DIR}/zconf.h.included)
  endif()
endif()

set(ZLIB_PC ${CMAKE_CURRENT_BINARY_DIR}/zlib.pc)
configure_file( ${PANDA_THIRD_PARTY_SOURCES_DIR}/zlib/zlib.pc.cmakein
		${ZLIB_PC} @ONLY)
configure_file(	${PANDA_THIRD_PARTY_SOURCES_DIR}/zlib/zconf.h.cmakein
		${CMAKE_CURRENT_BINARY_DIR}/zconf.h @ONLY)
include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_SOURCE_DIR})


#============================================================================
# zlib
#============================================================================

set(ZLIB_PUBLIC_HDRS
    ${CMAKE_CURRENT_BINARY_DIR}/zconf.h
    ${ZLIB_ROOT}/zlib.h
)
set(ZLIB_PRIVATE_HDRS
    ${ZLIB_ROOT}/crc32.h
    ${ZLIB_ROOT}/deflate.h
    ${ZLIB_ROOT}/gzguts.h
    ${ZLIB_ROOT}/inffast.h
    ${ZLIB_ROOT}/inffixed.h
    ${ZLIB_ROOT}/inflate.h
    ${ZLIB_ROOT}/inftrees.h
    ${ZLIB_ROOT}/trees.h
    ${ZLIB_ROOT}/zutil.h
)
set(ZLIB_SRCS
    ${ZLIB_ROOT}/adler32.c
    ${ZLIB_ROOT}/compress.c
    ${ZLIB_ROOT}/contrib/minizip/ioapi.c
    ${ZLIB_ROOT}/contrib/minizip/unzip.c
    ${ZLIB_ROOT}/contrib/minizip/zip.c
    ${ZLIB_ROOT}/crc32.c
    ${ZLIB_ROOT}/deflate.c
    ${ZLIB_ROOT}/gzclose.c
    ${ZLIB_ROOT}/gzlib.c
    ${ZLIB_ROOT}/gzread.c
    ${ZLIB_ROOT}/gzwrite.c
    ${ZLIB_ROOT}/inflate.c
    ${ZLIB_ROOT}/infback.c
    ${ZLIB_ROOT}/inftrees.c
    ${ZLIB_ROOT}/inffast.c
    ${ZLIB_ROOT}/trees.c
    ${ZLIB_ROOT}/uncompr.c
    ${ZLIB_ROOT}/zutil.c
)

if(NOT MINGW)
    set(ZLIB_DLL_SRCS
        ${ZLIB_ROOT}/win32/zlib1.rc # If present will override custom build rule below.
    )
endif()

if(CMAKE_COMPILER_IS_GNUCC)
    if(ASM686)
        set(ZLIB_ASMS  ${ZLIB_ROOT}/contrib/asm686/match.S)
    elseif (AMD64)
        set(ZLIB_ASMS  ${ZLIB_ROOT}/contrib/amd64/amd64-match.S)
    endif ()

	if(ZLIB_ASMS)
		add_definitions(-DASMV)
		set_source_files_properties(${ZLIB_ASMS} PROPERTIES LANGUAGE C COMPILE_FLAGS -DNO_UNDERLINE)
	endif()
endif()

if(MSVC)
    if(ASM686)
		ENABLE_LANGUAGE(ASM_MASM)
        set(ZLIB_ASMS
            ${ZLIB_ROOT}/contrib/masmx86/inffas32.asm
            ${ZLIB_ROOT}/contrib/masmx86/match686.asm
		)
    elseif (AMD64)
		ENABLE_LANGUAGE(ASM_MASM)
        set(ZLIB_ASMS
            ${ZLIB_ROOT}/contrib/masmx64/gvmat64.asm
            ${ZLIB_ROOT}/contrib/masmx64/inffasx64.asm
		)
    endif()

	if(ZLIB_ASMS)
		add_definitions(-DASMV -DASMINF)
	endif()
endif()

# parse the full version number from zlib.h and include in ZLIB_FULL_VERSION
file(READ ${PANDA_THIRD_PARTY_SOURCES_DIR}/zlib/zlib.h _zlib_h_contents)
string(REGEX REPLACE ".*#define[ \t]+ZLIB_VERSION[ \t]+\"([-0-9A-Za-z.]+)\".*"
    "\\1" ZLIB_FULL_VERSION ${_zlib_h_contents})

add_library(zlib STATIC ${ZLIB_SRCS} ${ZLIB_ASMS} ${ZLIB_PUBLIC_HDRS} ${ZLIB_PRIVATE_HDRS})
target_include_directories(zlib PUBLIC ${ZLIB_ROOT})

target_compile_options(zlib PUBLIC -Wno-return-type-c-linkage)

if(HOST_TOOLS)
    set_target_properties(zlib
        PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/third_party/zlib
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/third_party/zlib
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/third_party/zlib
    )
endif()

# NB! We always build zlib statically, but there seems
# no obvious reasons for that. If we reconsider, the logic
# below should be replaced with something like:
#
# add_library(zlib ${PANDA_DEFAULT_LIB_TYPE} zlib.c)
#
# **Besides** build of host tools should be fixed to
# take into account new shared library
if(NOT PANDA_TARGET_WINDOWS)
    target_compile_options(zlib PRIVATE -fPIC)
endif()

if (PANDA_ENABLE_AFL)
    include("${PANDA_ROOT}/fuzzing/Fuzzing.cmake")
    panda_substitute_libs(TARGET zlib)
endif()
