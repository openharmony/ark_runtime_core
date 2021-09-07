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

project(arkruntime)

include(core/Core.cmake)
include(${PANDA_ROOT}/verification/Verification.cmake)

function(gen_intrinsics_yaml)
    set(prefix ARG)
    set(noValues)
    set(singleValues OUTPUTFILE TARGET)
    set(multiValues DATAFILES DEPENDS)
    cmake_parse_arguments(${prefix}
                          "${noValues}"
                          "${singleValues}"
                          "${multiValues}"
                        ${ARGN})
    set(GENERATOR ${CMAKE_CURRENT_LIST_DIR}/templates/gen_intrinsics_data.rb ${CMAKE_CURRENT_LIST_DIR}/templates/runtime.rb)
    set(TEMPLATE ${CMAKE_CURRENT_LIST_DIR}/templates/intrinsics.yaml.erb)
    set(DEPENDS_LIST ${GENERATOR} ${TEMPLATE} ${ARG_DEPENDS})
    string(REPLACE ";" "," DATAFILES_STR "${ARG_DATAFILES}")
    foreach(d ${ARG_DATAFILES})
        list(APPEND DEPENDS_LIST ${d})
    endforeach()
    add_custom_command(OUTPUT ${ARG_OUTPUTFILE}
                       COMMENT "Generate intrinsics.yaml"
                       COMMAND ${GENERATOR} -d ${DATAFILES_STR} -t ${TEMPLATE} -o ${ARG_OUTPUTFILE}
                       DEPENDS ${DEPENDS_LIST})
    add_custom_target(${ARG_TARGET} ALL DEPENDS ${ARG_OUTPUTFILE})
endfunction()

set(SOURCES
    assert_gc_scope.cpp
    bridge/bridge.cpp
    entrypoints/entrypoints.cpp
    exceptions.cpp
    class_linker.cpp
    class_linker_extension.cpp
    class_initializer.cpp
    tooling/debugger.cpp
    tooling/pt_class.cpp
    tooling/pt_method.cpp
    tooling/pt_thread.cpp
    tooling/pt_reference.cpp
    tooling/pt_lang_extension.cpp
    tooling/debug_inf.cpp
    dyn_class_linker_extension.cpp
    field.cpp
    gc_task.cpp
    dprofiler/dprofiler.cpp
    interpreter/interpreter.cpp
    interpreter/runtime_interface.cpp
    intrinsics.cpp
    coretypes/string.cpp
    coretypes/array.cpp
    class.cpp
    class_helper.cpp
    locks.cpp
    panda_vm.cpp
    language_context.cpp
    mem/gc/epsilon/epsilon.cpp
    mem/gc/epsilon/epsilon_barrier.cpp
    mem/gc/gc.cpp
    mem/gc/lang/gc_lang.cpp
    mem/gc/static/gc_static_impl.cpp
    mem/gc/dynamic/gc_dynamic_impl.cpp
    mem/gc/gc_queue.cpp
    mem/gc/gc_root.cpp
    mem/gc/gc_stats.cpp
    mem/gc/gc_trigger.cpp
    mem/gc/card_table.cpp
    mem/gc/crossing_map.cpp
    mem/gc/crossing_map_singleton.cpp
    mem/gc/bitmap.cpp
    mem/gc/gc_scoped_phase.cpp
    mem/gc/stw-gc/stw-gc.cpp
    mem/gc/gc_barrier_set.cpp
    mem/gc/gen-gc/gen-gc.cpp
    mem/gc/hybrid-gc/hybrid_object_allocator.cpp
    mem/gc/reference-processor/reference_processor.cpp
    mem/refstorage/ref_block.cpp
    mem/refstorage/reference_storage.cpp
    mem/refstorage/global_object_storage.cpp
    mem/allocator.cpp
    mem/tlab.cpp
    mem/heap_manager.cpp
    mem/heap_verifier.cpp
    mem/rendezvous.cpp
    mem/runslots.cpp
    mem/region_space.cpp
    mem/object_helpers.cpp
    mem/mem_stats_additional_info.cpp
    mem/mem_stats_default.cpp
    mem/mem_stats.cpp
    mem/internal_allocator.cpp
    mem/panda_string.cpp
    mem/mem_hooks.cpp
    mem/memory_manager.cpp
    mark_word.cpp
    method.cpp
    monitor.cpp
    monitor_object_lock.cpp
    monitor_pool.cpp
    object_header.cpp
    runtime.cpp
    string_table.cpp
    thread.cpp
    thread_manager.cpp
    time_utils.cpp
    timing.cpp
    vtable_builder.cpp
    imtable_builder.cpp
    cframe.cpp
    stack_walker.cpp
    object_accessor.cpp
    arch/asm_support.cpp
    signal_handler.cpp
    profilesaver/profile_saver.cpp
    profilesaver/profile_dump_info.cpp
    runtime_helpers.cpp
    handle_scope.cpp
    file_manager.cpp
    mem/gc/generational-gc-base.cpp
    mem/gc/g1/g1-gc.cpp
    mem/gc/g1/g1-allocator.cpp)

if(PANDA_TARGET_ARM32_ABI_SOFT)
    list(APPEND SOURCES
        bridge/arch/arm/interpreter_to_compiled_code_bridge_arm.S
        bridge/arch/arm/compiled_code_to_interpreter_bridge_arm.S
        bridge/arch/arm/compiled_code_to_interpreter_bridge_dyn_arm.S
        bridge/arch/arm/interpreter_to_compiled_code_bridge_dyn_arm.S
        arch/arm/interpreter_support.S)
elseif (PANDA_TARGET_ARM32_ABI_HARD)
    list(APPEND SOURCES
        bridge/arch/arm/interpreter_to_compiled_code_bridge_armhf.S
        bridge/arch/arm/compiled_code_to_interpreter_bridge_armhf.S
        bridge/arch/arm/compiled_code_to_interpreter_bridge_dyn_arm.S
        bridge/arch/arm/interpreter_to_compiled_code_bridge_dyn_arm.S
        arch/arm/interpreter_support.S)
elseif(PANDA_TARGET_ARM64)
    list(APPEND SOURCES
        bridge/arch/aarch64/interpreter_to_compiled_code_bridge_aarch64.S
        bridge/arch/aarch64/compiled_code_to_interpreter_bridge_aarch64.S
        bridge/arch/aarch64/compiled_code_to_interpreter_bridge_dyn_aarch64.S
        bridge/arch/aarch64/interpreter_to_compiled_code_bridge_dyn_aarch64.S
        arch/aarch64/interpreter_support.S)
elseif(PANDA_TARGET_X86)
    list(APPEND SOURCES
        bridge/arch/x86/interpreter_to_compiled_code_bridge_x86.S
        bridge/arch/x86/compiled_code_to_interpreter_bridge_x86.S
        arch/x86/interpreter_support.S)
elseif(PANDA_TARGET_AMD64)
    list(APPEND SOURCES
        bridge/arch/amd64/compiled_code_to_interpreter_bridge_amd64.S
        bridge/arch/amd64/compiled_code_to_interpreter_bridge_dyn_amd64.S
        bridge/arch/amd64/interpreter_to_compiled_code_bridge_amd64.S
        bridge/arch/amd64/interpreter_to_compiled_code_bridge_dyn_amd64.S
        arch/amd64/interpreter_support.S)
endif()

set(INTERPRETER_IMPL_SOURCES
    interpreter/interpreter_impl.cpp
)

add_library(arkruntime_interpreter_impl OBJECT ${INTERPRETER_IMPL_SOURCES})
panda_set_lib_32bit_property(arkruntime_interpreter_impl)

set(ISA_TEMPLATES
    interpreter-inl_gen.h.erb
    isa_constants_gen.h.erb
    unimplemented_handlers-inl.h.erb
)
set(GEN_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/include")
file(MAKE_DIRECTORY ${GEN_INCLUDE_DIR})
panda_isa_gen(
    TEMPLATES ${ISA_TEMPLATES}
    SOURCE ${CMAKE_CURRENT_LIST_DIR}/interpreter/templates
    DESTINATION ${GEN_INCLUDE_DIR}
)

set(ISA "${CMAKE_BINARY_DIR}/isa/isa.yaml")
set(ISA_API "${PANDA_ROOT}/isa/isapi.rb")
set(BRIDGE_DISPATCH_TEMPLATE "${CMAKE_CURRENT_LIST_DIR}/templates/bridge_dispatch.S.erb")
set(BRIDGE_ARCHS aarch64 arm armhf amd64 x86)
foreach(arch ${BRIDGE_ARCHS})
    panda_gen_file(
        DATAFILE ${ISA}
        TEMPLATE ${BRIDGE_DISPATCH_TEMPLATE}
        OUTPUTFILE ${GEN_INCLUDE_DIR}/bridge_dispatch_${arch}.S
        REQUIRES ${ISA_API} ${CMAKE_CURRENT_LIST_DIR}/templates/bridge_helpers_${arch}.rb
                            ${CMAKE_CURRENT_LIST_DIR}/templates/bridge_helpers_static.rb
    )
    add_custom_target(bridge_dispatch_${arch} DEPENDS ${GEN_INCLUDE_DIR}/bridge_dispatch_${arch}.S ${ISA})
    add_dependencies(bridge_dispatch_${arch} isa_assert)

    panda_gen_file(
        DATAFILE ${ISA}
        TEMPLATE ${BRIDGE_DISPATCH_TEMPLATE}
        OUTPUTFILE ${GEN_INCLUDE_DIR}/bridge_dispatch_dyn_${arch}.S
        REQUIRES ${ISA_API} ${CMAKE_CURRENT_LIST_DIR}/templates/bridge_helpers_dynamic.rb ${CMAKE_CURRENT_LIST_DIR}/templates/bridge_helpers_${arch}.rb
    )
    add_custom_target(bridge_dispatch_dyn_${arch} DEPENDS ${GEN_INCLUDE_DIR}/bridge_dispatch_dyn_${arch}.S ${ISA})
    add_dependencies(bridge_dispatch_${arch} isa_assert)

endforeach()

set(GENERATOR ${PANDA_ROOT}/libpandafile/types.rb)
set(TEMPLATE ${CMAKE_CURRENT_LIST_DIR}/templates/shorty_values.h.erb)
set(DATAFILE ${PANDA_ROOT}/libpandafile/types.yaml)
set(DEPENDENCIES ${GENERATOR} ${TEMPLATE} ${DATAFILE})
set(OUTFILE "${CMAKE_CURRENT_BINARY_DIR}/include/shorty_values.h")
add_custom_command(OUTPUT "${OUTFILE}"
                   COMMENT "Generate shorty_values.h"
                   COMMAND ${PANDA_ROOT}/isa/gen.rb -d ${DATAFILE} -t ${TEMPLATE} -o "${OUTFILE}" -r ${GENERATOR}
                   DEPENDS ${DEPENDENCIES})
add_custom_target(shorty_values_gen_${PROJECT_NAME} ALL DEPENDS "${OUTFILE}")

target_compile_options(arkruntime_interpreter_impl PUBLIC -Wno-invalid-offsetof)

set_property(TARGET arkruntime_interpreter_impl PROPERTY POSITION_INDEPENDENT_CODE ON)

if (PANDA_ENABLE_GLOBAL_REGISTER_VARIABLES)
    if (PANDA_TARGET_ARM64)
        target_compile_options(arkruntime_interpreter_impl PUBLIC -ffixed-x20 -ffixed-x21 -ffixed-x22 -ffixed-x23 -ffixed-x24 -ffixed-x28)
        target_compile_definitions(arkruntime_interpreter_impl PUBLIC FFIXED_REGISTERS)
    endif()

    # Temporary disable clang tidy as it cannot recognize -ffixed-<reg> params that we used. Should be fixed in clang tidy 9
    set_target_properties(arkruntime_interpreter_impl PROPERTIES CXX_CLANG_TIDY "")
endif()

target_include_directories(arkruntime_interpreter_impl
    PRIVATE "$<TARGET_PROPERTY:c_secshared,INCLUDE_DIRECTORIES>"
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC ${PANDA_ROOT}
    PUBLIC ${GEN_INCLUDE_DIR}
    PUBLIC ${PANDA_ROOT}/libpandabase
    PUBLIC ${PANDA_ROOT}/libpandafile
    PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/panda_gen_options
    PUBLIC ${PANDA_BINARY_ROOT}/libpandafile/include
    PUBLIC ${PANDA_BINARY_ROOT}/libpandabase/generated
    PUBLIC ${VERIFIER_INCLUDE_DIR}
    PUBLIC ${VERIFIER_GEN_INCLUDE_DIR}
    PUBLIC ${PANDA_BINARY_ROOT}
)

add_dependencies(arkruntime_interpreter_impl
    isa_gen_${PROJECT_NAME}
    ${PROJECT_NAME}_static_options
    intrinsics_gen_${PROJECT_NAME}
    shorty_values_gen_${PROJECT_NAME}
    arkbase
    arkfile
)

set(INTRINSICS_YAML ${CMAKE_CURRENT_BINARY_DIR}/intrinsics.yaml)
set(INTRINSICS_TARGET ${PROJECT_NAME}_gen_intrinsics_yaml)
set(DATAFILES_TARGET ${CMAKE_CURRENT_LIST_DIR}/runtime.yaml)

gen_intrinsics_yaml(
    TARGET ${INTRINSICS_TARGET}
    DATAFILES ${DATAFILES_TARGET}
    OUTPUTFILE ${INTRINSICS_YAML}
)

set(RUNTIME_TEMPLATES
    intrinsics_gen.h.erb
    intrinsics.h.erb
    unimplemented_intrinsics-inl.cpp.erb
)

panda_gen(
    DATA ${INTRINSICS_YAML}
    TEMPLATES ${RUNTIME_TEMPLATES}
    REQUIRES ${CMAKE_CURRENT_LIST_DIR}/templates/intrinsics.rb
    SOURCE ${CMAKE_CURRENT_LIST_DIR}/templates
    DESTINATION ${GEN_INCLUDE_DIR}
    EXTRA_DEPENDENCIES ${INTRINSICS_TARGET}
)

add_library(arkruntime_static STATIC ${SOURCES} ${CORE_VM_SOURCES} ${VERIFIER_SOURCES} $<TARGET_OBJECTS:arkruntime_interpreter_impl>)
set_property(TARGET arkruntime_static PROPERTY POSITION_INDEPENDENT_CODE ON)

add_library(arkruntime SHARED $<TARGET_OBJECTS:arkruntime_interpreter_impl>)
target_link_libraries(arkruntime arkruntime_static)

panda_gen_options(
    TARGET arkruntime_static
    YAML_FILE options.yaml
    GENERATED_HEADER runtime_options_gen.h
)

panda_add_to_clang_tidy(TARGET arkruntime_static)

add_subdirectory(asm_defines)

add_dependencies(arkruntime_static
    intrinsics_gen_${PROJECT_NAME}
    isa_gen_pandaverification_gen
    messages_gen_pandaverification_gen
    arkstdlib
    asm_defines_generator
)

if(PANDA_TARGET_ARM32_ABI_SOFT)
    add_dependencies(arkruntime_static bridge_dispatch_arm bridge_dispatch_dyn_arm)
elseif (PANDA_TARGET_ARM32_ABI_HARD)
    add_dependencies(arkruntime_static bridge_dispatch_armhf bridge_dispatch_dyn_arm)
elseif(PANDA_TARGET_ARM64)
    add_dependencies(arkruntime_static bridge_dispatch_aarch64 bridge_dispatch_dyn_aarch64)
elseif(PANDA_TARGET_X86)
    add_dependencies(arkruntime_static bridge_dispatch_x86)
elseif(PANDA_TARGET_AMD64)
    add_dependencies(arkruntime_static bridge_dispatch_amd64 bridge_dispatch_dyn_amd64)
endif()

target_include_directories(arkruntime_static
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC ${PANDA_ROOT}
    PUBLIC ${GEN_INCLUDE_DIR}
    PUBLIC ${VERIFIER_INCLUDE_DIR}
    PUBLIC ${VERIFIER_GEN_INCLUDE_DIR}
)

target_link_libraries(arkruntime_static arkbase arkfile dprof)
if (NOT PANDA_TARGET_OHOS)
  target_link_libraries(arkruntime_static atomic)
endif()

# Disable warning about offsetof usage for non-standard layout types.
# In C++17 offsetof is conditionally implemented for such types, so
# compiler should issue error if it doesn't implemented offsetof for
# them. Also we use static asserts to ensure that offsetof works correcly
# for non-standard layout types.
target_compile_options(arkruntime_static PUBLIC -Wno-invalid-offsetof)

set(TEST_INTERPRETER_IMPL_SOURCES
    tests/interpreter/test_interpreter_impl.cpp
)

add_library(arkruntime_test_interpreter_impl OBJECT ${TEST_INTERPRETER_IMPL_SOURCES})

target_compile_options(arkruntime_test_interpreter_impl PUBLIC -Wno-invalid-offsetof)
panda_set_lib_32bit_property(arkruntime_test_interpreter_impl)

if (PANDA_ENABLE_GLOBAL_REGISTER_VARIABLES)
    if (PANDA_TARGET_ARM64)
        target_compile_options(arkruntime_test_interpreter_impl PUBLIC -ffixed-x20 -ffixed-x21 -ffixed-x22 -ffixed-x23 -ffixed-x24 -ffixed-x24 -ffixed-x28)
        target_compile_definitions(arkruntime_test_interpreter_impl PUBLIC FFIXED_REGISTERS)
    endif()

    # Temporary disable clang tidy as it cannot recognize -ffixed-<reg> params that we used. Should be fixed in clang tidy 9
    set_target_properties(arkruntime_test_interpreter_impl PROPERTIES CXX_CLANG_TIDY "")
endif()

target_include_directories(arkruntime_test_interpreter_impl
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC ${PANDA_ROOT}
    PUBLIC ${GEN_INCLUDE_DIR}
    PUBLIC ${PANDA_ROOT}/libpandabase
    PUBLIC ${PANDA_ROOT}/libpandafile
    PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/panda_gen_options
    PUBLIC ${PANDA_BINARY_ROOT}/libpandafile/include
    PUBLIC "$<TARGET_PROPERTY:c_secshared,INCLUDE_DIRECTORIES>"
    PUBLIC "$<TARGET_PROPERTY:gtest,INCLUDE_DIRECTORIES>"
    PUBLIC ${PANDA_BINARY_ROOT}/libpandabase/generated
    PUBLIC ${VERIFIER_INCLUDE_DIR}
    PUBLIC ${VERIFIER_GEN_INCLUDE_DIR}
)

add_dependencies(arkruntime_test_interpreter_impl
    isa_gen_${PROJECT_NAME}
    intrinsics_gen_${PROJECT_NAME}
    ${PROJECT_NAME}_static_options
    arkbase
    arkfile
)

if(PANDA_TARGET_ARM32_ABI_SOFT)
    SET(INVOKE_HELPER tests/arch/arm/invokation_helper.S)
elseif (PANDA_TARGET_ARM32_ABI_HARD)
    SET(INVOKE_HELPER tests/arch/arm/invokation_helper_hf.S)
elseif(PANDA_TARGET_ARM64)
    SET(INVOKE_HELPER tests/arch/aarch64/invokation_helper.S)
elseif(PANDA_TARGET_X86)
    #SET(INVOKE_HELPER tests/arch/x86/invokation_helper.S)
elseif(PANDA_TARGET_AMD64)
    SET(INVOKE_HELPER tests/arch/amd64/invokation_helper.S)
endif()

function(add_gtests test_name)
    panda_add_gtest(
        NO_CORES
        NAME ${test_name}
        SOURCES
            ${ARGN}
        LIBRARIES
            arkruntime
            arkassembler
        SANITIZERS
            ${PANDA_SANITIZERS_LIST}
    )

    if(TARGET ${test_name})
        target_include_directories(${test_name}
            PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
            PUBLIC ${PANDA_ROOT}
            PRIVATE ${GEN_INCLUDE_DIR}
        )
        target_compile_options(${test_name} PUBLIC "-Wno-ignored-attributes")

    endif()
endfunction()

function(add_multithread_gtests test_name)
    panda_add_gtest(
        NAME ${test_name}
        SOURCES
            ${ARGN}
        LIBRARIES
            arkruntime
            arkassembler
        SANITIZERS
            ${PANDA_SANITIZERS_LIST}
    )

    if(TARGET ${test_name})
        target_include_directories(${test_name}
            PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
            PUBLIC ${PANDA_ROOT}
            PRIVATE ${GEN_INCLUDE_DIR}
        )
        target_compile_options(${test_name} PUBLIC "-Wno-ignored-attributes")

    endif()
endfunction()

add_multithread_gtests (
    arkruntime_multithreaded_test
    tests/multithreaded_intern_string_table_test.cpp
)

add_gtests(
    arkruntime_bitmap_ClearRange_test
    tests/bitmap_clear_range_test.cpp
)

add_gtests(
    arkruntime_bitmap_OrderObjectAlignment_test
    tests/bitmap_order_object_alignment_test.cpp
)

add_gtests(
    arkruntime_bitmap_VisitorObjectAlignment_test
    tests/bitmap_visitor_object_alignment_test.cpp
)

add_gtests(
    arkruntime_bitmap_PageAlignment_test
    tests/bitmap_page_alignment_test.cpp
)

add_gtests(
    arkruntime_core_layout_test
    tests/array_test.cpp
    tests/mark_word_test.cpp
    tests/method_test.cpp
    tests/mock_queue_thread_pool.cpp
    tests/string_table_test.cpp
    tests/string_test.cpp
)

add_gtests(
    arkruntime_memory_mem_leak_test
    tests/mem_leak_test.cpp
)

add_gtests(
    arkruntime_memory_statistic_test
    tests/histogram_test.cpp
    tests/mem_stats_additional_info_test.cpp
    tests/mem_stats_gc_test.cpp
    tests/mem_stats_test.cpp
)

add_gtests(
    arkruntime_memory_management_bump_allocator_test
    tests/bump_allocator_test.cpp
)

add_gtests(
    arkruntime_memory_management_frame_allocator_test
    tests/frame_allocator_test.cpp
)

add_gtests(
    arkruntime_memory_management_freelist_allocator_test
    tests/freelist_allocator_test.cpp
)

add_gtests(
    arkruntime_memory_management_humongous_obj_allocator_test
    tests/humongous_obj_allocator_test.cpp
)

add_gtests(
    arkruntime_memory_management_internal_allocator_test
    tests/internal_allocator_test.cpp
)

add_gtests(
    arkruntime_memory_management_malloc-proxy-allocator-test
    tests/malloc-proxy-allocator-test.cpp
)

add_gtests(
    arkruntime_memory_management_pygote_space_allocator_gen_test
    tests/pygote_space_allocator_gen_test.cpp
)

add_gtests(
    arkruntime_memory_management_pygote_space_allocator_stw_test
    tests/pygote_space_allocator_stw_test.cpp
)

add_gtests(
    arkruntime_memory_management_region_allocator_test
    tests/region_allocator_test.cpp
)

add_gtests(
    arkruntime_memory_management_rem_set_test
    tests/rem_set_test.cpp
)

add_gtests(
    arkruntime_memory_management_runslots_allocator_test
    tests/runslots_allocator_test
)

add_gtests(
    arkruntime_memory_management_test_2
    tests/card_table_test.cpp
    tests/crossing_map_test.cpp
    tests/panda_smart_pointers_test.cpp
    tests/tlab_test.cpp
    tests/gc_task_test.cpp
)

add_gtests(
    arkruntime_multithreading_test
    tests/monitor_test.cpp
    tests/thread_test.cpp
)

add_gtests(
    arkruntime_options_test
    tests/options_test.cpp
)

add_gtests(
    arkruntime_interpreter_test
    tests/c2i_bridge_test.cpp
    tests/class_linker_test.cpp
    tests/class_linker_test_extension.cpp
    tests/hybrid_object_allocator_test.cpp
    tests/interpreter/test_interpreter.cpp
    tests/interpreter/test_runtime_interface.cpp
    tests/interpreter_test.cpp
    tests/invokation_helper.cpp
    $<TARGET_OBJECTS:arkruntime_test_interpreter_impl>
    ${INVOKE_HELPER}
)

add_gtests(
    arkruntime_utils_test
    tests/class_size_test.cpp
    tests/debugger_test.cpp
    tests/frame_test.cpp
    tests/i2c_bridge_test.cpp
    tests/math_helpers_test.cpp
    tests/offsets_test.cpp
    tests/time_utils_test.cpp
)

panda_add_sanitizers(TARGET arkruntime SANITIZERS ${PANDA_SANITIZERS_LIST})
panda_add_sanitizers(TARGET arkruntime_static SANITIZERS ${PANDA_SANITIZERS_LIST})
panda_add_sanitizers(TARGET arkruntime_interpreter_impl SANITIZERS ${PANDA_SANITIZERS_LIST})
panda_add_sanitizers(TARGET arkruntime_test_interpreter_impl SANITIZERS ${PANDA_SANITIZERS_LIST})

add_check_style(".")
