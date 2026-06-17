/*
 * Copyright (c) 2024 ec_msc contributors
 * SPDX-License-Identifier: MIT
 *
 * include/behaviors/ec_msc.h
 *
 * Direction token constants for use in keymap files.
 *
 * Example:
 *   #include <behaviors/ec_msc.h>
 *
 *   ec_msc: ec_msc {
 *       compatible = "zmk,behavior-ec-msc";
 *       #sensor-binding-cells = <2>;
 *   };
 *
 *   sensor-bindings = <&ec_msc U D>;
 */

#pragma once

/* Scroll direction tokens passed as binding parameters */
#define U 0   /* scroll up    */
#define D 1   /* scroll down  */
#define L 2   /* scroll left  */
#define R 3   /* scroll right */
