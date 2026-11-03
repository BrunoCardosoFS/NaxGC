cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)
option(NAXGC_BUILD_TESTS "Build NaxGC core unit tests" ON)

include(compilerconfig)
include(defaults)
include(helpers)
include(FetchContent)

set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")
FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
  SYSTEM
)
FetchContent_MakeAvailable(nlohmann_json)

add_library(naxgc_core STATIC)
target_sources(naxgc_core PRIVATE src/core/util/result.hpp src/core/util/color.hpp src/core/util/color.cpp)
target_sources(naxgc_core PRIVATE src/core/util/safe_path.hpp src/core/util/safe_path.cpp)
target_sources(naxgc_core PRIVATE src/core/manifest/manifest_types.hpp src/core/manifest/svg_inspector.hpp)
target_sources(naxgc_core PRIVATE src/core/manifest/svg_inspector.cpp src/core/manifest/manifest_parser.hpp)
target_sources(naxgc_core PRIVATE src/core/manifest/manifest_parser.cpp src/core/manifest/manifest_validator.hpp)
target_sources(naxgc_core PRIVATE src/core/manifest/manifest_validator.cpp)
target_include_directories(naxgc_core PUBLIC src)
target_link_libraries(naxgc_core PRIVATE nlohmann_json::nlohmann_json)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs naxgc_core)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE src)
target_include_directories(${CMAKE_PROJECT_NAME} SYSTEM PRIVATE third_party/obs-websocket)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
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

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

if(NAXGC_BUILD_TESTS)
  enable_testing()
  FetchContent_Declare(
    doctest
    URL https://github.com/doctest/doctest/archive/refs/tags/v2.4.11.tar.gz
    URL_HASH SHA256=632ed2c05a7f53fa961381497bf8069093f0d6628c5f26286161fbd32a560186
    SOURCE_SUBDIR cmake_header_only_ignore
    SYSTEM
  )
  FetchContent_MakeAvailable(doctest)

  add_executable(naxgc_core_tests tests/core/manifest_test.cpp)
  target_include_directories(naxgc_core_tests SYSTEM PRIVATE ${doctest_SOURCE_DIR})
  target_link_libraries(naxgc_core_tests PRIVATE naxgc_core)
  target_compile_definitions(naxgc_core_tests PRIVATE NAXGC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

  add_test(NAME naxgc_core_tests COMMAND naxgc_core_tests)
endif()
