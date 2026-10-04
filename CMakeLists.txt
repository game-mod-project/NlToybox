cmake_minimum_required(VERSION 3.21)
project(NlToyBox LANGUAGES CXX)

# 헤더는 YYToolkit 본체가 쓰는 정본을 쓴다. ExamplePlugin/include 의 사본은 내용이 다르다.
set(YYTK_ROOT "${CMAKE_SOURCE_DIR}/external/YYToolkit/YYToolkit")
set(YYTK_SHARED "${YYTK_ROOT}/source/YYTK/Shared")

if(NOT EXISTS "${YYTK_SHARED}/YYTK_Shared.hpp")
  message(FATAL_ERROR "external/YYToolkit 이 비어 있습니다. git submodule update --init 을 먼저 실행하세요.")
endif()

add_library(nltoybox SHARED
  src/ModuleMain.cpp
  src/Dump.cpp
  "${YYTK_SHARED}/YYTK_Shared_Types.cpp"
)

set_target_properties(nltoybox PROPERTIES
  OUTPUT_NAME "NlToyBox"
  PREFIX ""
  MSVC_RUNTIME_LIBRARY "MultiThreadedDLL"
)

target_include_directories(nltoybox PRIVATE
  "${YYTK_SHARED}"
  "${YYTK_ROOT}/include"
)

target_compile_options(nltoybox PRIVATE /std:c++latest /W3 /utf-8)
target_compile_definitions(nltoybox PRIVATE UNICODE _UNICODE)
