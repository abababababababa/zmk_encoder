/*
 * Copyright (c) 2024 ec_msc contributors
 * SPDX-License-Identifier: MIT
 *
 * behavior_ec_msc.c
 *
 * EC11 encoder scroll behavior for ZMK (main / v0.4+).
 *
 * Sends a single HID Consumer scroll event (press + immediate release) per
 * encoder notch so that lost sensor events never leave a "stuck" key state.
 *
 * Keymap usage:
 *   sensor-bindings = <&ec_msc U D>, <&ec_msc L R>;
 *
 * Direction tokens (U D L R) map to MOVE_UP / MOVE_DOWN / MOVE_LEFT / MOVE_RIGHT.
 */

#define DT_DRV_COMPAT zmk_behavior_ec_msc

#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zmk/hid.h>
#include <zmk/endpoints.h>
#include <zmk/keymap.h>
#include <zmk/mouse/types.h>
#include <zmk/mouse/hid.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/* ── Direction constants ──────────────────────────────────────────── */
#define EC_MSC_U 0
#define EC_MSC_D 1
#define EC_MSC_L 2
#define EC_MSC_R 3

/* ── DTS instance data ────────────────────────────────────────────── */
struct behavior_ec_msc_config {
    /* nothing per-instance; directions come from binding params */
};

/* ── Helper: send one scroll tick and release immediately ─────────── */
static int send_scroll(uint8_t direction) {
    int8_t x = 0, y = 0;

    switch (direction) {
    case EC_MSC_U: y =  1; break;
    case EC_MSC_D: y = -1; break;
    case EC_MSC_L: x = -1; break;
    case EC_MSC_R: x =  1; break;
    default:
        LOG_WRN("ec_msc: unknown direction %d", direction);
        return -EINVAL;
    }

    /* Acquire the HID scroll state, send, then immediately clear.
     * zmk_hid_mouse_scroll_set() / zmk_hid_mouse_scroll_clear() are the
     * public API in ZMK main for wheel scroll (v0.4+).            */
    zmk_hid_mouse_scroll_set(x, y);
    int err = zmk_endpoints_send_mouse_report();
    zmk_hid_mouse_scroll_clear();
    if (err) {
        LOG_ERR("ec_msc: send_mouse_report failed (%d)", err);
        return err;
    }

    /* Send the cleared (zero) report so the host sees a release. */
    err = zmk_endpoints_send_mouse_report();
    if (err) {
        LOG_ERR("ec_msc: send_mouse_report (clear) failed (%d)", err);
    }
    return err;
}

/* ── Sensor binding handler (called by ZMK per encoder event) ──────── */
static int on_sensor_binding_triggered(struct zmk_behavior_binding *binding,
                                       const struct zmk_behavior_sensor_val *val,
                                       enum sensor_channel channel) {
    /*
     * binding->param1 = direction for CW  (right/up rotation)
     * binding->param2 = direction for CCW (left/down rotation)
     *
     * ZMK v0.4 passes the sensor increment via val->value.
     * Positive value → CW, negative value → CCW.
     */
    uint8_t direction;

    if (val->value > 0) {
        direction = (uint8_t)binding->param1;
    } else if (val->value < 0) {
        direction = (uint8_t)binding->param2;
    } else {
        /* zero increment – nothing to do */
        return ZMK_BEHAVIOR_OPAQUE;
    }

    /* Fire one scroll tick and immediately release. */
    int err = send_scroll(direction);
    if (err < 0) {
        return err;
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

/* ── Behavior API vtable ────────────────────────────────────────────── */
static const struct behavior_driver_api behavior_ec_msc_driver_api = {
    .sensor_binding_triggered = on_sensor_binding_triggered,
};

/* ── Device instantiation macro ─────────────────────────────────────── */
#define EC_MSC_INST(n)                                                          \
    static const struct behavior_ec_msc_config behavior_ec_msc_config_##n = {}; \
    BEHAVIOR_DT_INST_DEFINE(n,                                                  \
                            NULL,                                               \
                            NULL,                                               \
                            NULL,                                               \
                            &behavior_ec_msc_config_##n,                        \
                            POST_KERNEL,                                        \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                \
                            &behavior_ec_msc_driver_api);

DT_INST_FOREACH_STATUS_OKAY(EC_MSC_INST)
