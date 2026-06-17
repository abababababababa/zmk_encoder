/*
 * Copyright (c) 2024 ec_msc contributors
 * SPDX-License-Identifier: MIT
 *
 * behavior_ec_msc.c
 *
 * EC11 encoder scroll behavior for ZMK main (Zephyr 4.1 / ZMK post-PR#2477).
 *
 * Uses the two-phase sensor API introduced alongside the input subsystem:
 *   sensor_binding_accept_data  — receives raw sensor_value, stores direction
 *   sensor_binding_process      — fires input_report_rel and returns
 *
 * Because press+release happen in one call stack, a lost sensor event can
 * never leave a "stuck" scroll state.
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
#include <zmk/sensors.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/* ── Direction tokens (must match include/behaviors/ec_msc.h) ──────── */
#define EC_MSC_U 0
#define EC_MSC_D 1
#define EC_MSC_L 2
#define EC_MSC_R 3

/* ── Per-instance data: stores direction resolved in accept_data ────── */
struct behavior_ec_msc_data {
    uint8_t direction; /* EC_MSC_U/D/L/R resolved from last accept_data */
    bool    pending;   /* true when a scroll tick should be sent */
};

struct behavior_ec_msc_config {
    /* nothing; params come from the binding at runtime */
};

/* ── send one scroll tick via Zephyr input subsystem ─────────────────── */
static int send_scroll(uint8_t direction)
{
    uint16_t code;
    int32_t  value;

    switch (direction) {
    case EC_MSC_U: code = INPUT_REL_WHEEL;  value =  1; break;
    case EC_MSC_D: code = INPUT_REL_WHEEL;  value = -1; break;
    case EC_MSC_R: code = INPUT_REL_HWHEEL; value =  1; break;
    case EC_MSC_L: code = INPUT_REL_HWHEEL; value = -1; break;
    default:
        LOG_WRN("ec_msc: unknown direction %d", direction);
        return -EINVAL;
    }

    /* sync=true flushes the event immediately — no separate sync call needed */
    int err = input_report_rel(NULL, code, value, true, K_NO_WAIT);
    if (err) {
        LOG_ERR("ec_msc: input_report_rel failed (%d)", err);
    }
    return err;
}

/* ── Phase 1: receive sensor data, resolve direction ─────────────────── */
static int on_sensor_binding_accept_data(
    struct zmk_behavior_binding *binding,
    struct zmk_behavior_binding_event event,
    const struct zmk_sensor_config *sensor_config,
    size_t channel_data_size,
    const struct zmk_sensor_channel_data channel_data[channel_data_size])
{
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    struct behavior_ec_msc_data *data = dev->data;

    if (channel_data_size == 0) {
        data->pending = false;
        return 0;
    }

    /* val1 carries the rotation increment (positive = CW, negative = CCW) */
    int32_t inc = channel_data[0].value.val1;

    if (inc == 0) {
        data->pending = false;
        return 0;
    }

    data->direction = (inc > 0) ? (uint8_t)binding->param1
                                : (uint8_t)binding->param2;
    data->pending   = true;
    return 0;
}

/* ── Phase 2: fire the scroll event (or discard) ─────────────────────── */
static int on_sensor_binding_process(
    struct zmk_behavior_binding *binding,
    struct zmk_behavior_binding_event event,
    enum behavior_sensor_binding_process_mode mode)
{
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    struct behavior_ec_msc_data *data = dev->data;

    if (!data->pending) {
        return ZMK_BEHAVIOR_OPAQUE;
    }

    data->pending = false;

    if (mode == BEHAVIOR_SENSOR_BINDING_PROCESS_MODE_DISCARD) {
        return ZMK_BEHAVIOR_OPAQUE;
    }

    int err = send_scroll(data->direction);
    return (err < 0) ? err : ZMK_BEHAVIOR_OPAQUE;
}

/* ── Behavior driver API vtable ──────────────────────────────────────── */
static const struct behavior_driver_api behavior_ec_msc_driver_api = {
    .sensor_binding_accept_data = on_sensor_binding_accept_data,
    .sensor_binding_process     = on_sensor_binding_process,
};

/* ── Instantiation macro ─────────────────────────────────────────────── */
#define EC_MSC_INST(n)                                                           \
    static struct behavior_ec_msc_data behavior_ec_msc_data_##n = {};           \
    static const struct behavior_ec_msc_config behavior_ec_msc_config_##n = {}; \
    BEHAVIOR_DT_INST_DEFINE(n,                                                   \
                            NULL,                                                \
                            NULL,                                                \
                            &behavior_ec_msc_data_##n,                          \
                            &behavior_ec_msc_config_##n,                        \
                            POST_KERNEL,                                         \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                 \
                            &behavior_ec_msc_driver_api);

DT_INST_FOREACH_STATUS_OKAY(EC_MSC_INST)
