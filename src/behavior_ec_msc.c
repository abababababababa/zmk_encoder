/*
 * Copyright (c) 2024 ec_msc contributors
 * SPDX-License-Identifier: MIT
 *
 * behavior_ec_msc.c
 *
 * EC11 encoder scroll behavior for ZMK (main / v0.4+ / Zephyr 4.x).
 *
 * Sends a single HID scroll event per encoder notch using Zephyr's
 * input subsystem (input_report_rel + input_report_sync).
 * Press and release are completed in the same call stack, so a lost
 * sensor event can never leave a "stuck" scroll state.
 *
 * Keymap usage:
 *   sensor-bindings = <&ec_msc U D>;   // CW=Up,    CCW=Down
 *   sensor-bindings = <&ec_msc R L>;   // CW=Right, CCW=Left
 */

#define DT_DRV_COMPAT zmk_behavior_ec_msc

#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>
#include <drivers/behavior.h>
#include <zmk/behavior.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/* ── Direction tokens (must match include/behaviors/ec_msc.h) ──────── */
#define EC_MSC_U 0
#define EC_MSC_D 1
#define EC_MSC_L 2
#define EC_MSC_R 3

/* ── Per-instance config (nothing needed; all state is in params) ───── */
struct behavior_ec_msc_config {
};

/* ── send_scroll: fire one tick and sync immediately ────────────────── */
static int send_scroll(uint8_t direction)
{
    /*
     * ZMK main uses Zephyr's input subsystem for pointing devices.
     * input_report_rel() queues a REL event; input_report_sync()
     * flushes it.  Two syncs (non-zero then zero) give the host a
     * clean press-then-release within a single function call.
     *
     * INPUT_REL_WHEEL  = vertical   scroll  (positive = up)
     * INPUT_REL_HWHEEL = horizontal scroll  (positive = right)
     */
    uint16_t code;
    int32_t  value;

    switch (direction) {
    case EC_MSC_U:
        code  = INPUT_REL_WHEEL;
        value = 1;
        break;
    case EC_MSC_D:
        code  = INPUT_REL_WHEEL;
        value = -1;
        break;
    case EC_MSC_R:
        code  = INPUT_REL_HWHEEL;
        value = 1;
        break;
    case EC_MSC_L:
        code  = INPUT_REL_HWHEEL;
        value = -1;
        break;
    default:
        LOG_WRN("ec_msc: unknown direction %d", direction);
        return -EINVAL;
    }

    /* Send the scroll value and sync (= "press") */
    int err = input_report_rel(NULL, code, value, true, K_NO_WAIT);
    if (err) {
        LOG_ERR("ec_msc: input_report_rel failed (%d)", err);
    }
    return err;
}

/* ── Sensor binding handler ─────────────────────────────────────────── */
static int on_sensor_binding_triggered(struct zmk_behavior_binding *binding,
                                       const struct zmk_behavior_sensor_val *val,
                                       enum sensor_channel channel)
{
    /*
     * param1 = direction for CW  (positive increment)
     * param2 = direction for CCW (negative increment)
     */
    if (val->value == 0) {
        return ZMK_BEHAVIOR_OPAQUE;
    }

    uint8_t direction = (val->value > 0)
                        ? (uint8_t)binding->param1
                        : (uint8_t)binding->param2;

    int err = send_scroll(direction);
    return (err < 0) ? err : ZMK_BEHAVIOR_OPAQUE;
}

/* ── Behavior driver API ─────────────────────────────────────────────── */
static const struct behavior_driver_api behavior_ec_msc_driver_api = {
    .sensor_binding_triggered = on_sensor_binding_triggered,
};

/* ── Instantiation macro ─────────────────────────────────────────────── */
#define EC_MSC_INST(n)                                                           \
    static const struct behavior_ec_msc_config behavior_ec_msc_config_##n = {}; \
    BEHAVIOR_DT_INST_DEFINE(n,                                                   \
                            NULL,                                                \
                            NULL,                                                \
                            NULL,                                                \
                            &behavior_ec_msc_config_##n,                         \
                            POST_KERNEL,                                         \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                 \
                            &behavior_ec_msc_driver_api);

DT_INST_FOREACH_STATUS_OKAY(EC_MSC_INST)