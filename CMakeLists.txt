# Copyright (c) 2024 ec_msc contributors
# SPDX-License-Identifier: MIT
#
# Top-level CMakeLists.txt for the zmk-behavior-ec-msc ZMK module.

cmake_minimum_required(VERSION 3.20)

# Make the module's include directory available to ZMK so that
# #include <behaviors/ec_msc.h> works from keymap .h includes.
if(DEFINED ZMK_EXTRA_MODULES)
  # When loaded as a ZMK module the include path is registered via
  # zephyr_include_directories() below.
endif()

zephyr_include_directories(include)

add_subdirectory(src)
