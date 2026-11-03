cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)
option(NAXGC_BUILD_TESTS "Build NaxGC core unit tests" ON)

include(compilerconfig)
include(defaults)
include(helpers)

if(MSVC)
  if(POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
  endif()
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "Embedded")
  string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
  string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
  string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
  string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
endif()

include(FetchContent)

set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")
set(LUNASVG_BUILD_EXAMPLES OFF CACHE INTERNAL "")
set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
# gersemi: off
FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
  SYSTEM
  EXCLUDE_FROM_ALL
)
FetchContent_Declare(
  lunasvg
  URL https://github.com/sammycage/lunasvg/archive/refs/tags/v2.4.1.tar.gz
  URL_HASH SHA256=db9d2134c8c2545694e71e62fb0772a7d089fe53e1ace1e08c2279a89e450534
  SYSTEM
  EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(nlohmann_json lunasvg)
# gersemi: on

target_compile_definitions(lunasvg PUBLIC LUNASVG_BUILD_STATIC)
set_property(TARGET lunasvg PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET lunasvg PROPERTY COMPILE_WARNING_AS_ERROR OFF)
if(MSVC)
  target_compile_options(lunasvg PRIVATE /W0)
else()
  target_compile_options(lunasvg PRIVATE -w)
endif()

add_library(naxgc_core STATIC)
set_property(TARGET naxgc_core PROPERTY POSITION_INDEPENDENT_CODE ON)
target_sources(naxgc_core PRIVATE src/core/util/result.hpp src/core/util/color.hpp src/core/util/color.cpp)
target_sources(naxgc_core PRIVATE src/core/util/mat2d.hpp src/core/util/safe_path.hpp src/core/util/safe_path.cpp)
target_sources(naxgc_core PRIVATE src/core/manifest/manifest_types.hpp src/core/manifest/svg_inspector.hpp)
target_sources(naxgc_core PRIVATE src/core/manifest/svg_inspector.cpp src/core/manifest/manifest_parser.hpp)
target_sources(naxgc_core PRIVATE src/core/manifest/manifest_parser.cpp src/core/manifest/manifest_validator.hpp)
target_sources(naxgc_core PRIVATE src/core/manifest/manifest_validator.cpp)
target_sources(naxgc_core PRIVATE src/core/scene/scene.hpp src/core/scene/scene.cpp)
target_sources(naxgc_core PRIVATE src/core/anim/easing.hpp src/core/anim/easing.cpp)
target_sources(naxgc_core PRIVATE src/core/anim/interpolator.hpp src/core/anim/interpolator.cpp)
target_sources(naxgc_core PRIVATE src/core/anim/track_evaluator.hpp src/core/anim/track_evaluator.cpp)
target_sources(naxgc_core PRIVATE src/core/anim/clip_instance.hpp src/core/anim/clip_instance.cpp)
target_sources(naxgc_core PRIVATE src/core/anim/action_scheduler.hpp src/core/anim/action_scheduler.cpp)
target_sources(naxgc_core PRIVATE src/core/anim/animator.hpp src/core/anim/animator.cpp)
target_include_directories(naxgc_core PUBLIC src)
target_link_libraries(naxgc_core PRIVATE nlohmann_json::nlohmann_json)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs naxgc_core lunasvg)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE src)
target_include_directories(${CMAKE_PROJECT_NAME} SYSTEM PRIVATE third_party/obs-websocket)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Gui Core REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/plugin-main.cpp src/obs/naxgc_source.hpp src/obs/naxgc_source.cpp)
target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/render/rasterizer_svg.hpp src/render/rasterizer_svg.cpp)
target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/render/rasterizer_text.hpp src/render/rasterizer_text.cpp)
target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/render/texture_cache.hpp src/render/texture_cache.cpp)
target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/render/renderer.hpp src/render/renderer.cpp)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

if(NAXGC_BUILD_TESTS)
  enable_testing()
  # gersemi: off
  FetchContent_Declare(
    doctest
    URL https://github.com/doctest/doctest/archive/refs/tags/v2.4.11.tar.gz
    URL_HASH SHA256=632ed2c05a7f53fa961381497bf8069093f0d6628c5f26286161fbd32a560186
    SOURCE_SUBDIR cmake_header_only_ignore
    SYSTEM
  )
  FetchContent_MakeAvailable(doctest)
  # gersemi: on

  add_executable(
    naxgc_core_tests
    tests/test_main.cpp
    tests/core/manifest_test.cpp
    tests/core/scene_test.cpp
    tests/core/rasterizer_test.cpp
    tests/core/easing_test.cpp
    tests/core/animator_test.cpp
    tests/core/golden_test.cpp
  )
  target_sources(naxgc_core_tests PRIVATE src/render/rasterizer_svg.hpp src/render/rasterizer_svg.cpp)
  target_sources(naxgc_core_tests PRIVATE src/render/rasterizer_text.hpp src/render/rasterizer_text.cpp)
  target_include_directories(naxgc_core_tests PRIVATE src)
  target_include_directories(naxgc_core_tests SYSTEM PRIVATE ${doctest_SOURCE_DIR})
  target_link_libraries(naxgc_core_tests PRIVATE naxgc_core lunasvg Qt6::Core Qt6::Gui)
  target_compile_definitions(naxgc_core_tests PRIVATE NAXGC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

  add_test(NAME naxgc_core_tests COMMAND naxgc_core_tests)
  if(WIN32)
    # gersemi: off
    set_tests_properties(
      naxgc_core_tests
      PROPERTIES
        ENVIRONMENT_MODIFICATION
          "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;QT_PLUGIN_PATH=set:$<TARGET_FILE_DIR:Qt6::Core>/../plugins"
    )
    # gersemi: on
  elseif(APPLE)
    set(_qt_lib_dir "${Qt6_DIR}/../..")
    cmake_path(NORMAL_PATH _qt_lib_dir)
    set(_qt_plugin_dir "${Qt6_DIR}/../../../plugins")
    cmake_path(NORMAL_PATH _qt_plugin_dir)

    set(_macos_test_rpaths "${_qt_lib_dir}")
    foreach(_prefix IN LISTS CMAKE_PREFIX_PATH)
      if(EXISTS "${_prefix}/lib")
        list(APPEND _macos_test_rpaths "${_prefix}/lib")
      endif()
      if(EXISTS "${_prefix}/Frameworks")
        list(APPEND _macos_test_rpaths "${_prefix}/Frameworks")
      endif()
    endforeach()
    list(REMOVE_DUPLICATES _macos_test_rpaths)

    foreach(_rpath IN LISTS _macos_test_rpaths)
      target_link_options(naxgc_core_tests PRIVATE "LINKER:-rpath,${_rpath}")
    endforeach()

    # gersemi: off
    set_target_properties(
      naxgc_core_tests
      PROPERTIES
        XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME NO
        "XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME[variant=Release]" NO
        "XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME[variant=RelWithDebInfo]" NO
        "XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME[variant=MinSizeRel]" NO
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "-"
    )
    set_tests_properties(
      naxgc_core_tests
      PROPERTIES
        ENVIRONMENT_MODIFICATION
          "DYLD_FRAMEWORK_PATH=path_list_prepend:${_qt_lib_dir};QT_PLUGIN_PATH=set:${_qt_plugin_dir}"
    )
    # gersemi: on
  endif()
endif()
