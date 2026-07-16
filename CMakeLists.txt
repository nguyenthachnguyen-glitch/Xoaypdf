cmake_minimum_required(VERSION 3.21)
cmake_policy(SET CMP0091 NEW)
project(XoayPDF CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Link tinh runtime MSVC de exe chay doc lap, khong can cai VC++ Redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

find_package(qpdf CONFIG REQUIRED)

add_executable(XoayPDF XoayPDF.cpp)

target_compile_options(XoayPDF PRIVATE /utf-8 /EHsc)
target_compile_definitions(XoayPDF PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    POINTERHOLDER_TRANSITION=0
)
target_link_libraries(XoayPDF PRIVATE qpdf::libqpdf WindowsApp.lib)
