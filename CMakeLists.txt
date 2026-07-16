cmake_minimum_required(VERSION 3.21)
cmake_policy(SET CMP0091 NEW)
project(XoayPDF CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

find_path(QPDF_INCLUDE_DIR qpdf/QPDF.hh REQUIRED)
find_library(QPDF_LIBRARY NAMES qpdf libqpdf REQUIRED)

# Link toan bo .lib tinh vcpkg da cai (deps cua qpdf); lib thua se bi linker bo qua
get_filename_component(VCPKG_LIB_DIR "${QPDF_LIBRARY}" DIRECTORY)
file(GLOB VCPKG_ALL_LIBS "${VCPKG_LIB_DIR}/*.lib")
message(STATUS "Libs se link: ${VCPKG_ALL_LIBS}")

add_executable(XoayPDF XoayPDF.cpp)
target_include_directories(XoayPDF PRIVATE ${QPDF_INCLUDE_DIR})
target_compile_options(XoayPDF PRIVATE /utf-8 /EHsc)
target_compile_definitions(XoayPDF PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX POINTERHOLDER_TRANSITION=0)
target_link_libraries(XoayPDF PRIVATE ${VCPKG_ALL_LIBS} WindowsApp.lib)
