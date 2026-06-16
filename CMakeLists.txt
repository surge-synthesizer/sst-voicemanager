cmake_minimum_required(VERSION 3.10)
project(sst-voicemanager VERSION 0.5 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)

add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE include)
target_compile_definitions(${PROJECT_NAME} INTERFACE _USE_MATH_DEFINES=1)

set(bldtest FALSE)
if (DEFINED SST_VOICEMANAGER_BUILD_TESTS)
  set(bldtest ${SST_VOICEMANAGER_BUILD_TESTS})
endif()
if (PROJECT_IS_TOP_LEVEL)
  if (NOT TARGET sst-cpputils)
      message(STATUS "Adding CPPUTILS library with CPM")
      include(cmake/CPM.cmake)
      CPMAddPackage(NAME sst-cpputils
                GITHUB_REPOSITORY surge-synthesizer/sst-cpputils
                GIT_TAG main
      )
  endif ()
  set(bldtest TRUE)
  if (USE_RTSAN)
    message(STATUS "Activating RTSAN")
    add_compile_options(-fsanitize=realtime)
    add_compile_definitions(USE_RTSAN=TRUE)
    add_link_options(-fsanitize=realtime)
  endif()
else()
    if (NOT TARGET sst-cpputils)
        message(FATAL_ERROR "sst-voicemanager now requires sst-cpputils. Please stage it before including voicemanager")
    endif()
endif()

target_link_libraries(${PROJECT_NAME} INTERFACE sst-cpputils)

if (MSVC)
    target_compile_options(${PROJECT_NAME} INTERFACE /Zc:__cplusplus)
endif ()

if (${bldtest})

    add_executable(${PROJECT_NAME}-test
            tests/main.cpp
            tests/basic.cpp

            tests/basic_poly.cpp
            tests/mono_playback.cpp
            tests/playmode_change.cpp
            tests/mpe_tests.cpp
            tests/routing_params.cpp
            tests/stealing_groups.cpp
            tests/hierarchical_groups.cpp
            tests/stealing_maxvoice.cpp
            tests/stealing_priorities.cpp
            tests/priority_mono.cpp
            tests/legato_playback.cpp
            tests/piano_mode.cpp
            tests/midi1_to_vm.cpp
            tests/note_ids.cpp
            tests/note_on_return.cpp
            tests/voice_ids.cpp
            tests/continuation_data.cpp
            tests/multi_port.cpp
        )

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${PROJECT_NAME}-test PRIVATE -Wall -Wextra -Wno-unused-parameter -Wpedantic -Werror)
    endif()
    target_link_libraries(${PROJECT_NAME}-test PRIVATE ${PROJECT_NAME})
    target_include_directories(${PROJECT_NAME}-test PRIVATE libs/catch2)
endif ()
