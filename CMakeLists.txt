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
  src/core/AskPath.cpp
  src/core/CallLog.cpp
  src/core/CheatState.cpp
  src/core/CheatTable.cpp
  src/core/CostBook.cpp
  src/core/CourtPlan.cpp
  src/core/CrimePlan.cpp
  src/core/DiplomacyPlan.cpp
  src/core/EconomyPlan.cpp
  src/core/JobTally.cpp
  src/core/Knobs.cpp
  src/core/NumberEdit.cpp
  src/core/Localization.cpp
  src/core/PeoplePlan.cpp
  src/core/Presets.cpp
  src/core/FamilyPlan.cpp
  src/core/Rate.cpp
  src/core/RolePlan.cpp
  src/core/SeasonPlan.cpp
  src/core/RemoteCommand.cpp
  src/core/Text.cpp
  src/core/WorldPlan.cpp
  src/core/Request.cpp
  src/core/Schedule.cpp
)
set_target_properties(nlcore PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
target_compile_options(nlcore PUBLIC /std:c++latest /W3 /utf-8)

# src/core 의 시험. 게임을 켜지 않는다. tools/test-native.ps1 이 돌린다.
add_executable(nlcore_tests
  tests/native/main.cpp
  tests/native/test_text.cpp
  tests/native/test_request.cpp
  tests/native/test_schedule.cpp
  tests/native/test_askpath.cpp
  tests/native/test_knobs.cpp
  tests/native/test_economy.cpp
  tests/native/test_cheat.cpp
  tests/native/test_costbook.cpp
  tests/native/test_hooks.cpp
  tests/native/test_rate.cpp
  tests/native/test_remote.cpp
  tests/native/test_people.cpp
  tests/native/test_presets.cpp
  tests/native/test_world.cpp
  tests/native/test_diplomacy.cpp
  tests/native/test_court.cpp
  tests/native/test_localization.cpp
  tests/native/test_family.cpp
  tests/native/test_role.cpp
  tests/native/test_crime.cpp
)
set_target_properties(nlcore_tests PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
target_include_directories(nlcore_tests PRIVATE src)
target_link_libraries(nlcore_tests PRIVATE nlcore)

# 모드창은 Dear ImGui 로 그린다(서브모듈 external/imgui, MIT).
set(IMGUI_ROOT "${CMAKE_SOURCE_DIR}/external/imgui")
if(NOT EXISTS "${IMGUI_ROOT}/imgui.h")
  message(FATAL_ERROR "external/imgui 가 비어 있습니다. git submodule update --init 을 먼저 실행하세요.")
endif()

add_library(nltoybox SHARED
  src/ModuleMain.cpp
  src/Access.cpp
  src/Build.cpp
  src/Buildings.cpp
  src/Production.cpp
  src/Jobs.cpp
  src/Cheats.cpp
  src/Court.cpp
  src/Crime.cpp
  src/Dump.cpp
  src/Economy.cpp
  src/Explorer.cpp
  src/Finder.cpp
  src/Game.cpp
  src/Menu.cpp
  src/People.cpp
  src/PeopleAccess.cpp
  src/TraitText.cpp
  src/Recorder.cpp
  src/Remote.cpp
  src/Search.cpp
  src/Tweaks.cpp
  src/Ui.cpp
  src/Diplomacy.cpp
  src/World.cpp
  src/Season.cpp
  src/Mines.cpp
  "${YYTK_SHARED}/YYTK_Shared_Types.cpp"
  "${IMGUI_ROOT}/imgui.cpp"
  "${IMGUI_ROOT}/imgui_draw.cpp"
  "${IMGUI_ROOT}/imgui_tables.cpp"
  "${IMGUI_ROOT}/imgui_widgets.cpp"
  "${IMGUI_ROOT}/backends/imgui_impl_dx11.cpp"
  "${IMGUI_ROOT}/backends/imgui_impl_win32.cpp"
)

set_target_properties(nltoybox PROPERTIES
  OUTPUT_NAME "NlToyBox"
  PREFIX ""
  MSVC_RUNTIME_LIBRARY "MultiThreadedDLL"
)

target_include_directories(nltoybox PRIVATE
  "${YYTK_SHARED}"
  "${YYTK_ROOT}/include"
  "${IMGUI_ROOT}"
)

target_compile_options(nltoybox PRIVATE /std:c++latest /W3 /utf-8)
# 게임패드로 모드창을 조작하지 않는다(XInput 을 끌어오지 않는다).
target_compile_definitions(nltoybox PRIVATE UNICODE _UNICODE IMGUI_IMPL_WIN32_DISABLE_GAMEPAD)
target_link_libraries(nltoybox PRIVATE nlcore d3d11 dxgi d3dcompiler)
