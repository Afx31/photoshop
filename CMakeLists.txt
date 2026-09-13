cmake_minimum_required(VERSION 3.16)
project(photo VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Gui)
find_package(OpenMP)
find_package(PkgConfig REQUIRED)
pkg_check_modules(HEIF REQUIRED IMPORTED_TARGET libheif)
pkg_check_modules(JXL REQUIRED IMPORTED_TARGET libjxl)
pkg_check_modules(JXLTHREADS REQUIRED IMPORTED_TARGET libjxl_threads)

add_executable(photo
  src/main.cpp
  src/window.cpp
  src/viewer.cpp
  src/panel.cpp
  src/filmstrip.cpp
  src/presets.cpp
  src/rawio.cpp
  src/imageutil.cpp
  src/engine.c
)

target_include_directories(photo PRIVATE src)
target_link_libraries(photo PRIVATE
  Qt6::Widgets
  Qt6::Gui
  PkgConfig::HEIF
  PkgConfig::JXL
  PkgConfig::JXLTHREADS
)

if(OpenMP_C_FOUND)
  target_link_libraries(photo PRIVATE OpenMP::OpenMP_C)
endif()

set_target_properties(photo PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}
)

set_source_files_properties(src/engine.c PROPERTIES COMPILE_FLAGS "-O3 -ffast-math")
