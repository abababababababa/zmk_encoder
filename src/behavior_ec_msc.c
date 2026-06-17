/*
 * Copyright (c) 2024 ec_msc contributors
 * SPDX-License-Identifier: MIT
 *
 * behavior_ec_msc.c
 *
 * EC11 encoder scroll behavior for ZMK main (Zephyr 4.1).
 *
 * On Central (CONFIG_ZMK_KEYMAP=y):
 *   accept_data resolves direction → process queues &msc press+release.
 *
 * On Peripheral (CONFIG_ZMK_KEYMAP=n):
 *   accept_data stores direction (for split forwarding by ZMK core),
 *   process is a no-op — zmk_behavior_queue_add is not linked on peripheral.
 *
 * Keymap usage:
 *   sensor-bindings = <&ec_msc U D>;   // CW=Up,    CCW=Down
 *   sensor-bindings = <&ec_msc R L>;   // CW=Right, CCW=Left
 */

#define DT_DRV_COMPAT zmk_behavior_ec_msc

#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zmk/sensors.h>

#if IS_ENABLED(CONFIG_ZMK_KEYMAP)
#include <zmk/behavior_queue.h>
#include <dt-bindings/zmk/pointing.h>
#endif

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/* ── Direction tokens (must match include/behaviors/ec_msc.h) ──────── */
#define EC_MSC_U 0
#define EC_MSC_D 1
#define EC_MSC_L 2
#define EC_MSC_R 3

/* ── Per-instance data ───────────────────────────────────────────────── */
struct behavior_ec_msc_data {
    uint8_t direction;
    bool    pending;
};

struct behavior_ec_msc_config {
};

/* ── Phase 1: accept_data — decode direction from sensor channel ──────── */
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

/* ── Phase 2: process — queue &msc on Central, no-op on Peripheral ────── */
static int on_sensor_binding_process(
    struct zmk_behavior_binding *binding,
    struct zmk_behavior_binding_event event,
    enum behavior_sensor_binding_process_mode mode)
{
#if IS_ENABLED(CONFIG_ZMK_KEYMAP)
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    struct behavior_ec_msc_data *data = dev->data;

    if (!data->pending) {
        return ZMK_BEHAVIOR_OPAQUE;
    }
    data->pending = false;

    if (mode == BEHAVIOR_SENSOR_BINDING_PROCESS_MODE_DISCARD) {
        return ZMK_BEHAVIOR_OPAQUE;
    }

    uint32_t param;
    switch (data->direction) {
    case EC_MSC_U: param = SCRL_UP;    break;
    case EC_MSC_D: param = SCRL_DOWN;  break;
    case EC_MSC_L: param = SCRL_LEFT;  break;
    case EC_MSC_R: param = SCRL_RIGHT; break;
    default:       param = SCRL_DOWN;  break;
    }

    struct zmk_behavior_binding msc_binding = {
        .behavior_dev = "msc",
        .param1       = param,
        .param2       = 0,
    };

    zmk_behavior_queue_add(&event, msc_binding, true,  0);
    zmk_behavior_queue_add(&event, msc_binding, false, 0);
#endif /* IS_ENABLED(CONFIG_ZMK_KEYMAP) */

    return ZMK_BEHAVIOR_OPAQUE;
}

/* ── Behavior driver API ─────────────────────────────────────────────── */
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
