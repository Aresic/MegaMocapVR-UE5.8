cmake_minimum_required(VERSION 3.21)
project(MMVR_FingerProbe VERSION 0.0.1 LANGUAGES CXX)
if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "V0 requires Windows x64 and MSVC")
endif()
set(SDK "${CMAKE_CURRENT_SOURCE_DIR}/third_party/openvr-1.5.17")
add_executable(MMVR_FingerProbe src/main.cpp)
target_compile_features(MMVR_FingerProbe PRIVATE cxx_std_17)
target_include_directories(MMVR_FingerProbe SYSTEM PRIVATE "${SDK}/headers")
target_compile_definitions(MMVR_FingerProbe PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(MMVR_FingerProbe PRIVATE /W4 /WX /utf-8 /permissive-)
set_property(TARGET MMVR_FingerProbe PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_link_libraries(MMVR_FingerProbe PRIVATE "${SDK}/lib/win64/openvr_api.lib" psapi ws2_32 ole32)
set_target_properties(MMVR_FingerProbe PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/$<0:>")
add_custom_command(TARGET MMVR_FingerProbe POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SDK}/bin/win64/openvr_api.dll" "$<TARGET_FILE_DIR:MMVR_FingerProbe>/openvr_api.dll"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/config" "$<TARGET_FILE_DIR:MMVR_FingerProbe>/config"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SDK}/LICENSE" "$<TARGET_FILE_DIR:MMVR_FingerProbe>/OPENVR_LICENSE.txt"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/NOTICE.md" "$<TARGET_FILE_DIR:MMVR_FingerProbe>/NOTICE.md")
