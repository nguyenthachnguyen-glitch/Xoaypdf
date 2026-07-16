cmake_minimum_required(VERSION 3.21)
cmake_policy(SET CMP0091 NEW)
project(XoayPDF CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Link tinh runtime MSVC de exe chay doc lap
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Tim truc tiep thu vien do vcpkg cai, khong dung target export cua qpdf (loi zs.lib)
find_path(QPDF_INCLUDE_DIR qpdf/QPDF.hh REQUIRED)
find_library(QPDF_LIBRARY NAMES qpdf libqpdf REQUIRED)
find_library(ZLIB_LIBRARY NAMES zlib z zlibstatic REQUIRED)
find_library(JPEG_LIBRARY NAMES jpeg libjpeg turbojpeg jpeg-static REQUIRED)

add_executable(XoayPDF XoayPDF.cpp)
target_include_directories(XoayPDF PRIVATE ${QPDF_INCLUDE_DIR})
target_compile_options(XoayPDF PRIVATE /utf-8 /EHsc)
target_compile_definitions(XoayPDF PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX POINTERHOLDER_TRANSITION=0)
target_link_libraries(XoayPDF PRIVATE ${QPDF_LIBRARY} ${ZLIB_LIBRARY} ${JPEG_LIBRARY} WindowsApp.lib)
