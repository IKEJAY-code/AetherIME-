cmake_minimum_required(VERSION 3.20)

project(shurufa_tsf_shell LANGUAGES CXX)

add_library(shurufa_tsf_shell SHARED
  src/ClassFactory.cpp
  src/DisplayAttribute.cpp
  src/DllMain.cpp
  src/Guids.cpp
  src/IpcClient.cpp
  src/Log.cpp
  src/Registry.cpp
  src/TsfGuids.cpp
  src/TextService.cpp
  shurufa_tsf_shell.def
)

target_include_directories(shurufa_tsf_shell PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(shurufa_tsf_shell PRIVATE cxx_std_20)
target_compile_definitions(shurufa_tsf_shell PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
if(MSVC)
  target_compile_options(shurufa_tsf_shell PRIVATE /utf-8)
endif()

target_link_libraries(shurufa_tsf_shell PRIVATE
  ole32
  advapi32
  uuid
  ws2_32
)

# Put output under build/tsf_shell/<platform>/bin/<config> so scripts can find it.
set_target_properties(shurufa_tsf_shell PROPERTIES
  OUTPUT_NAME "shurufa_tsf_shell"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/Debug"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release"
)
