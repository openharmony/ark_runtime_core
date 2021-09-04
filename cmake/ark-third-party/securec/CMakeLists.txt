# Copyright (c) 2021 Huawei Device Co., Ltd.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.10)

project(securec)

set(SOURCES
    ${SECUREC_ROOT}/src/vsprintf_s.c
    ${SECUREC_ROOT}/src/wmemmove_s.c
    ${SECUREC_ROOT}/src/strncat_s.c
    ${SECUREC_ROOT}/src/vsnprintf_s.c
    ${SECUREC_ROOT}/src/fwscanf_s.c
    ${SECUREC_ROOT}/src/scanf_s.c
    ${SECUREC_ROOT}/src/strcat_s.c
    ${SECUREC_ROOT}/src/sscanf_s.c
    ${SECUREC_ROOT}/src/secureprintoutput_w.c
    ${SECUREC_ROOT}/src/wmemcpy_s.c
    ${SECUREC_ROOT}/src/wcsncat_s.c
    ${SECUREC_ROOT}/src/secureprintoutput_a.c
    ${SECUREC_ROOT}/src/secureinput_w.c
    ${SECUREC_ROOT}/src/memcpy_s.c
    ${SECUREC_ROOT}/src/fscanf_s.c
    ${SECUREC_ROOT}/src/vswscanf_s.c
    ${SECUREC_ROOT}/src/secureinput_a.c
    ${SECUREC_ROOT}/src/sprintf_s.c
    ${SECUREC_ROOT}/src/memmove_s.c
    ${SECUREC_ROOT}/src/swscanf_s.c
    ${SECUREC_ROOT}/src/snprintf_s.c
    ${SECUREC_ROOT}/src/vscanf_s.c
    ${SECUREC_ROOT}/src/vswprintf_s.c
    ${SECUREC_ROOT}/src/wcscpy_s.c
    ${SECUREC_ROOT}/src/vfwscanf_s.c
    ${SECUREC_ROOT}/src/memset_s.c
    ${SECUREC_ROOT}/src/wscanf_s.c
    ${SECUREC_ROOT}/src/vwscanf_s.c
    ${SECUREC_ROOT}/src/strtok_s.c
    ${SECUREC_ROOT}/src/wcsncpy_s.c
    ${SECUREC_ROOT}/src/vfscanf_s.c
    ${SECUREC_ROOT}/src/vsscanf_s.c
    ${SECUREC_ROOT}/src/wcstok_s.c
    ${SECUREC_ROOT}/src/securecutil.c
    ${SECUREC_ROOT}/src/gets_s.c
    ${SECUREC_ROOT}/src/swprintf_s.c
    ${SECUREC_ROOT}/src/strcpy_s.c
    ${SECUREC_ROOT}/src/wcscat_s.c
    ${SECUREC_ROOT}/src/strncpy_s.c
)

add_library(c_secshared ${PANDA_DEFAULT_LIB_TYPE} ${SOURCES})
target_include_directories(c_secshared PUBLIC ${SECUREC_ROOT}/include)
target_compile_definitions(c_secshared PRIVATE NDEBUG SECUREC_SUPPORT_STRTOLD=1)
if (PANDA_TARGET_WINDOWS)
    target_compile_definitions(c_secshared PUBLIC _CRTBLD __LIBMSVCRT__)
endif()
target_compile_options(c_secshared PRIVATE -Wall -O1)

if (PANDA_ENABLE_AFL)
    add_library(c_secshared_fuzz ${PANDA_DEFAULT_LIB_TYPE} ${SOURCES})
    target_include_directories(c_secshared_fuzz PUBLIC ${SECUREC_ROOT}/include)
    target_compile_definitions(c_secshared_fuzz PRIVATE NDEBUG SECUREC_SUPPORT_STRTOLD=1)
    if (PANDA_TARGET_WINDOWS)
        target_compile_definitions(c_secshared_fuzz PUBLIC _CRTBLD __LIBMSVCRT__)
    endif()
    target_compile_options(c_secshared_fuzz PRIVATE -Wall -O1 --afl_noopt)
endif()
