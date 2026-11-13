cmake_minimum_required(VERSION 3.24)
project(DGGS_HALO_QUERY VERSION 1.2.0 LANGUAGES CXX)

include(CTest)

option(BAFE_ENABLE_CUDA "Build CUDA experiments" ON)
option(BAFE_ENABLE_H3 "Build the H3 programs (needs an installed H3 CMake package)" OFF)
option(BAFE_ENABLE_S2 "Build the S2 programs (needs an installed S2 CMake package)" OFF)
option(BAFE_REQUIRE_CUDA "Fail configuration when CUDA is requested but unavailable" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_definitions(_USE_MATH_DEFINES)
endif()

if(BAFE_ENABLE_CUDA)
  if(NOT CMAKE_CUDA_COMPILER)
    if(BAFE_REQUIRE_CUDA)
      message(FATAL_ERROR
        "BAFE_ENABLE_CUDA=ON but CMAKE_CUDA_COMPILER is empty. "
        "Use the supplied Windows build script, which selects Ninja Multi-Config "
        "and passes the absolute nvcc.exe path.")
    else()
      set(BAFE_ENABLE_CUDA OFF)
    endif()
  endif()
endif()

if(BAFE_ENABLE_CUDA)
  enable_language(CUDA)
  find_package(CUDAToolkit REQUIRED)
  set(CMAKE_CUDA_STANDARD 17)
  set(CMAKE_CUDA_STANDARD_REQUIRED ON)
  set(CMAKE_CUDA_EXTENSIONS OFF)
  if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
    set(CMAKE_CUDA_ARCHITECTURES 89 CACHE STRING "CUDA architectures")
  endif()
  message(STATUS "CUDA compiler: ${CMAKE_CUDA_COMPILER}")
  message(STATUS "CUDA toolkit: ${CUDAToolkit_ROOT}")
  message(STATUS "CUDA architectures: ${CMAKE_CUDA_ARCHITECTURES}")
endif()

add_subdirectory(bafe)

add_library(bafe_exp_headers INTERFACE)
target_include_directories(bafe_exp_headers INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/bafe/src
  ${CMAKE_CURRENT_SOURCE_DIR}/bafe/include)

# Minimal example of the library interface.
add_executable(halo_example examples/minimal_halo.cpp)
target_link_libraries(halo_example PRIVATE bafe::surface bafe_exp_headers)

add_executable(bafe_synthetic_benchmark benchmarks/bafe_synthetic_benchmark.cpp)
target_link_libraries(bafe_synthetic_benchmark PRIVATE bafe::surface bafe_exp_headers)

add_executable(bafe_hierarchy_benchmark benchmarks/bafe_hierarchy_benchmark.cpp)
target_link_libraries(bafe_hierarchy_benchmark PRIVATE bafe::surface bafe_exp_headers)

add_executable(bafe_dual_index_ablation benchmarks/bafe_dual_index_ablation.cpp)
target_link_libraries(bafe_dual_index_ablation PRIVATE bafe::surface bafe_exp_headers)

add_executable(bafe_rhombus_cpu_ablation benchmarks/bafe_rhombus_cpu_ablation.cpp)
target_link_libraries(bafe_rhombus_cpu_ablation PRIVATE bafe::surface bafe_exp_headers)

add_executable(bafe_geographic_mapper benchmarks/bafe_geographic_mapper.cpp)
target_link_libraries(bafe_geographic_mapper PRIVATE bafe::surface bafe_exp_headers)

add_executable(bafe_canada_regression benchmarks/bafe_canada_regression.cpp)
target_link_libraries(bafe_canada_regression PRIVATE bafe::surface bafe_exp_headers)

add_executable(bafe_calgary_clearance benchmarks/bafe_calgary_clearance.cpp)
target_link_libraries(bafe_calgary_clearance PRIVATE bafe::surface bafe_exp_headers)

add_executable(bafe_realworld_halo benchmarks/bafe_realworld_halo.cpp benchmarks/process_control.cpp)
target_link_libraries(bafe_realworld_halo PRIVATE bafe::surface bafe_exp_headers)

# Revision 2026-11: representation study on one CPU thread and the global ring
# validation of every implementation.
add_executable(bafe_representation_ablation benchmarks/bafe_representation_ablation.cpp benchmarks/process_control.cpp)
target_link_libraries(bafe_representation_ablation PRIVATE bafe::surface bafe_exp_headers)

add_executable(bafe_global_validation benchmarks/bafe_global_validation.cpp)
target_link_libraries(bafe_global_validation PRIVATE bafe::surface bafe_exp_headers)

add_executable(bafe_spatial_semantics benchmarks/bafe_spatial_semantics.cpp)
target_link_libraries(bafe_spatial_semantics PRIVATE bafe::surface bafe_exp_headers)

if(BAFE_ENABLE_CUDA)
  add_executable(bafe_cuda_ablations benchmarks/bafe_cuda_ablations.cu)
  target_link_libraries(bafe_cuda_ablations PRIVATE bafe::surface bafe_exp_headers CUDA::cudart)
  target_include_directories(bafe_cuda_ablations PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/bafe/src)
  set_target_properties(bafe_cuda_ablations PROPERTIES
    CUDA_SEPARABLE_COMPILATION ON
    CUDA_RESOLVE_DEVICE_SYMBOLS ON)
  if(MSVC)
    target_compile_options(bafe_cuda_ablations PRIVATE
      $<$<COMPILE_LANGUAGE:CUDA>:-O3 -Xcompiler=/O2,/EHsc,/Zc:preprocessor>)
  endif()
endif()

if(BAFE_ENABLE_H3)
  find_package(h3 CONFIG REQUIRED)
  set(H3_TARGET "")
  foreach(t h3::h3 h3 h3-static)
    if(TARGET ${t})
      set(H3_TARGET ${t})
      break()
    endif()
  endforeach()
  if(NOT H3_TARGET)
    message(FATAL_ERROR "H3 package was found but no supported CMake target was exported")
  endif()

  add_executable(h3_native_benchmark benchmarks/h3_native_benchmark.cpp)
  target_link_libraries(h3_native_benchmark PRIVATE ${H3_TARGET} bafe_exp_headers)

  add_executable(h3_hierarchy_benchmark benchmarks/h3_hierarchy_benchmark.cpp)
  target_link_libraries(h3_hierarchy_benchmark PRIVATE ${H3_TARGET} bafe_exp_headers)

  add_executable(h3_calgary_clearance benchmarks/h3_calgary_clearance.cpp)
  target_link_libraries(h3_calgary_clearance PRIVATE ${H3_TARGET} bafe_exp_headers)

  # Union of H3 grid disk calls (context comparison of the paper, record C).
  add_executable(h3_grid_disk_union benchmarks/h3_grid_disk_union.cpp)
  target_link_libraries(h3_grid_disk_union PRIVATE ${H3_TARGET} bafe_exp_headers)
endif()

if(BAFE_ENABLE_S2)
  find_package(s2 CONFIG REQUIRED)
  set(S2_TARGET "")
  foreach(t s2::s2 S2::s2 s2)
    if(TARGET ${t})
      set(S2_TARGET ${t})
      break()
    endif()
  endforeach()
  if(NOT S2_TARGET)
    message(FATAL_ERROR "S2 package was found but no supported CMake target was exported")
  endif()

  add_executable(s2_native_benchmark benchmarks/s2_native_benchmark.cpp)
  target_link_libraries(s2_native_benchmark PRIVATE ${S2_TARGET} bafe_exp_headers)

  add_executable(s2_hierarchy_benchmark benchmarks/s2_hierarchy_benchmark.cpp)
  target_link_libraries(s2_hierarchy_benchmark PRIVATE ${S2_TARGET} bafe_exp_headers)

  add_executable(s2_calgary_clearance benchmarks/s2_calgary_clearance.cpp)
  target_link_libraries(s2_calgary_clearance PRIVATE ${S2_TARGET} bafe_exp_headers)
endif()
