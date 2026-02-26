cmake_minimum_required(VERSION 3.21)
project(aetherime VERSION 0.1.0 LANGUAGES C CXX)

option(AETHERIME_BUILD_FCITX5 "Build Fcitx5 addon" ON)

if(AETHERIME_BUILD_FCITX5)
  add_subdirectory(fcitx5)
endif()
