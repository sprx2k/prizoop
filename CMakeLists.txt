cmake_minimum_required(VERSION 3.15)
project(prizoop LANGUAGES C CXX ASM VERSION "01.01.0000")

set(PLATFORM "cw" CACHE STRING "Target Casio platform: cw (fx-CG100 / Graph Math+) or cg (fx-CG50)")
if(NOT "${PLATFORM}" STREQUAL "cw" AND NOT "${PLATFORM}" STREQUAL "cg")
  message(FATAL_ERROR "Unsupported PLATFORM='${PLATFORM}'. Must be 'cw' or 'cg'.")
endif()

include(GenerateG3A)

configure_file(src/config.h.in include/config.h)
add_compile_definitions(TARGET_PRIZM=1 DEBUG=0)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.S")

include_directories(
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_BINARY_DIR}/include")

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/utils/zx7"            "utils/zx7")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/utils/calctype"       "utils/calctype")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/utils/snd"            "utils/snd")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/utils/scope_timer"    "utils/scope_timer")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/utils/ptune2_simple"  "utils/ptune2_simple")

add_executable(prizoop ${SOURCES})

target_compile_options(prizoop PRIVATE
  -O2 -Wall -funroll-loops -fno-trapping-math -fno-trapv -Wno-switch
  -ffunction-sections -fdata-sections
  $<$<COMPILE_LANGUAGE:C>:-std=c99>
  $<$<COMPILE_LANGUAGE:CXX>:-std=c++11 -fpermissive -fno-rtti
    -fno-exceptions -fno-threadsafe-statics
    -fno-use-cxa-get-exception-ptr>)

target_link_options(prizoop PRIVATE
  -L "${CMAKE_CURRENT_SOURCE_DIR}"
  -T "${CMAKE_CURRENT_SOURCE_DIR}/prizm.ld"
  -Wl,-Map=map -Wl,-print-memory-usage
  -Wl,-gc-sections)

target_link_libraries(prizoop PRIVATE
  zx7 calctype snd scope_timer ptune2_simple
  -lsyscall-${PLATFORM} -lc -lopenlibm -lgcc)

if("${FXSDK_PLATFORM_LONG}" STREQUAL fxCG50 OR "${FXSDK_PLATFORM_LONG}" STREQUAL fxCG100)
  if("${PLATFORM}" STREQUAL cw)
    generate_g3a(TARGET prizoop OUTPUT "prizoop_cw.g3a"
      NAME "Prizoop" ICONS icon-uns.png icon-sel.png)
  else()
    generate_g3a(TARGET prizoop OUTPUT "prizoop_cg.g3a"
      NAME "Prizoop" ICONS icon-uns.png icon-sel.png)
  endif()
endif()