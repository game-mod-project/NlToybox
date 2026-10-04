cmake_minimum_required(VERSION 3.21)
project(NlToyBox LANGUAGES CXX)

# 헤더는 YYToolkit 본체가 쓰는 정본을 쓴다. ExamplePlugin/include 의 사본은 내용이 다르다.
set(YYTK_ROOT "${CMAKE_SOURCE_DIR}/external/YYToolkit/YYToolkit")
set(YYTK_SHARED "${YYTK_ROOT}/source/YYTK/Shared")

if(NOT EXISTS "${YYTK_SHARED}/YYTK_Shared.hpp")
  message(FATAL_ERROR "external/YYToolkit 이 비어 있습니다. git submodule update --init 을 먼저 실행하세요.")
endif()

# 러너에 기대지 않는 부분. 모듈과 네이티브 시험이 같이 쓴다.
add_library(nlcore STATIC
  src/core/Text.cpp
  src/core/Request.cpp
  src/core/Schedule.cpp
)
set_target_properties(nlcore PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
target_compile_options(nlcore PUBLIC /std:c++latest /W3 /utf-8)

# src/core 의 시험. 게임을 켜지 않는다. tools/test-native.ps1 이 돌린다.
add_executable(nlcore_tests tests/native/core_tests.cpp)
set_target_properties(nlcore_tests PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
target_include_directories(nlcore_tests PRIVATE src)
target_link_libraries(nlcore_tests PRIVATE nlcore)

add_library(nltoybox SHARED
  src/ModuleMain.cpp
  src/Dump.cpp
  src/Finder.cpp
  src/Game.cpp
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
target_link_libraries(nltoybox PRIVATE nlcore)
