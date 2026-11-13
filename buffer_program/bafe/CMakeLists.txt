cmake_minimum_required(VERSION 3.24)

option(BAFE_ENABLE_CUDA "Build CUDA hash-frontier backend" ON)
option(BAFE_BUILD_TESTS "Build BAFE correctness tests" ON)

set(BAFE_SOURCES
  src/factory.cpp
  src/halo_cpu.cpp)

if(BAFE_ENABLE_CUDA)
  if(NOT CMAKE_CUDA_COMPILER_LOADED)
    message(FATAL_ERROR
      "The parent project must enable the CUDA language before adding bafe/. "
      "Run the supplied Windows build script.")
  endif()
  list(APPEND BAFE_SOURCES src/halo_cuda.cu)
else()
  list(APPEND BAFE_SOURCES src/halo_cuda_stub.cpp)
endif()

add_library(bafe_surface STATIC ${BAFE_SOURCES})
add_library(bafe::surface ALIAS bafe_surface)

target_include_directories(bafe_surface
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_compile_definitions(bafe_surface
  PUBLIC BAFE_HAS_CUDA=$<BOOL:${BAFE_ENABLE_CUDA}>
  PRIVATE BAFE_VERSION="1.0.0")

if(BAFE_ENABLE_CUDA)
  target_link_libraries(bafe_surface PUBLIC CUDA::cudart)
  set_target_properties(bafe_surface PROPERTIES
    CUDA_SEPARABLE_COMPILATION ON
    CUDA_RESOLVE_DEVICE_SYMBOLS ON)
endif()

if(MSVC)
  target_compile_options(bafe_surface PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:/O2 /EHsc /Zc:preprocessor>
    $<$<COMPILE_LANGUAGE:CUDA>:-O3 -Xcompiler=/O2,/EHsc,/Zc:preprocessor>)
else()
  target_compile_options(bafe_surface PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-O3>
    $<$<COMPILE_LANGUAGE:CUDA>:-O3>)
endif()

if(BAFE_BUILD_TESTS)
  add_executable(bafe_surface_tests tests/bafe_tests.cpp)
  target_link_libraries(bafe_surface_tests PRIVATE bafe::surface)
  target_include_directories(bafe_surface_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  add_test(NAME bafe_surface_correctness COMMAND bafe_surface_tests)
endif()
