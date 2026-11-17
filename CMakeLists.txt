cmake_minimum_required(VERSION 3.28)
project(BlackHoleSim VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

option(BH_ENABLE_CUDA "Build the CUDA ray-tracing backend" ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

if(BH_ENABLE_CUDA)
  enable_language(CUDA)
  set(CMAKE_CUDA_STANDARD 17)
  set(CMAKE_CUDA_STANDARD_REQUIRED ON)
  set(CMAKE_CUDA_ARCHITECTURES 86)
endif()

# ---- Qt ----
if(NOT DEFINED QT_PREFIX)
  set(QT_PREFIX "D:/Qt/6.8.3/msvc2022_64")
endif()
list(APPEND CMAKE_PREFIX_PATH "${QT_PREFIX}")
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets)

# ---- core (physics, QtCore only for containers/json) ----
add_library(bh_core STATIC
  src/core/scene.cpp
  src/core/sim.cpp
  src/core/presets.cpp
  src/core/renderparams.cpp
)
target_include_directories(bh_core PUBLIC src)
target_link_libraries(bh_core PUBLIC Qt6::Core Qt6::Gui)
target_compile_definitions(bh_core PUBLIC _USE_MATH_DEFINES NOMINMAX)

# ---- imaging: CUDA when available, otherwise the CPU reference tracer ----
if(BH_ENABLE_CUDA)
  find_package(CUDAToolkit REQUIRED)
  add_library(bh_imaging STATIC
    src/imaging/raytracer.cu
    src/imaging/render_host.cpp
  )
  target_include_directories(bh_imaging PUBLIC src)
  target_link_libraries(bh_imaging PUBLIC bh_core CUDA::cudart)
  set_target_properties(bh_imaging PROPERTIES CUDA_SEPARABLE_COMPILATION ON)
  target_compile_definitions(bh_imaging PUBLIC BH_HAS_CUDA=1)
else()
  add_library(bh_imaging STATIC src/imaging/cpu_fallback.cpp)
  target_include_directories(bh_imaging PUBLIC src)
  target_link_libraries(bh_imaging PUBLIC bh_core)
endif()

# ---- tests ----
add_executable(bh_tests tests/test_main.cpp)
target_link_libraries(bh_tests PRIVATE bh_core)

# ---- headless renderer ----
add_executable(bh_cli tools/render_cli.cpp)
target_link_libraries(bh_cli PRIVATE bh_core bh_imaging Qt6::Gui)

# ---- application ----
# moc is invoked directly instead of through CMake AUTOMOC: on this machine
# AUTOMOC's --output-dep-file step intermittently blocks (antivirus scanning
# of the freshly written .ddi file), which stalls incremental builds.
set(APP_HEADERS
  src/ui/mainwindow.h
  src/ui/glview.h
  src/ui/imageview.h
  src/ui/researchpanel.h
  src/ui/addbodydialog.h
)
set(APP_MOC_SOURCES "")
foreach(hdr ${APP_HEADERS})
  get_filename_component(_name ${hdr} NAME_WE)
  set(_out "${CMAKE_CURRENT_BINARY_DIR}/moc/moc_${_name}.cpp")
  add_custom_command(
    OUTPUT ${_out}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/moc"
    COMMAND Qt6::moc "${CMAKE_CURRENT_SOURCE_DIR}/${hdr}" -o "${_out}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${hdr}"
    COMMENT "moc ${_name}"
    VERBATIM
  )
  list(APPEND APP_MOC_SOURCES ${_out})
endforeach()

qt_add_executable(BlackHoleSimulator
  src/app/main.cpp
  src/ui/mainwindow.cpp
  src/ui/glview.cpp
  src/ui/imageview.cpp
  src/ui/researchpanel.cpp
  src/ui/addbodydialog.cpp
  ${APP_MOC_SOURCES}
)
set_target_properties(BlackHoleSimulator PROPERTIES AUTOMOC OFF)
target_link_libraries(BlackHoleSimulator PRIVATE
  bh_core bh_imaging
  Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets
)
