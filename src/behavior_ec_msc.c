/*
 * Copyright (c) 2024 ec_msc contributors
 * SPDX-License-Identifier: MIT
 *
 * behavior_ec_msc.c
 */

#define DT_DRV_COMPAT zmk_behavior_ec_msc

#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zmk/behavior_queue.h>
#include <zmk/sensors.h>
#include <dt-bindings/zmk/pointing.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/* ── Direction tokens ──────── */
#define EC_MSC_U 0
#define EC_MSC_D 1
#define EC_MSC_L 2
#define EC_MSC_R 3

static uint32_t direction_to_msc_param(uint8_t direction)
{
    switch (direction) {
    case EC_MSC_U: return SCRL_UP;
    case EC_MSC_D: return SCRL_DOWN;
    case EC_MSC_L: return SCRL_LEFT;
    case EC_MSC_R: return SCRL_RIGHT;
    default:       return SCRL_DOWN;
    }
}

struct behavior_ec_msc_data {
    uint8_t direction;
    bool    pending;
};

struct behavior_ec_msc_config {};

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

    uint32_t param = direction_to_msc_param(data->direction);

    /* 【修正】デバイス名は元の "msc" に戻す */
    struct zmk_behavior_binding msc_binding = {
        .behavior_dev = "msc",
        .param1       = param,
        .param2       = 0,
    };

#if IS_ENABLED(CONFIG_ZMK_SPLIT) && !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    /* Peripheral側は何もしない（パケットロス問題回避の要） */
    return ZMK_BEHAVIOR_OPAQUE;
#else
    /* Central側で、押す(0ms)と離す(15ms)をアトミックにキューに積む */
    /* これによりBLEのロスによるStuckバグは絶対に起きず、かつラグも最小限になります */
    return zmk_behavior_queue_add(&event, msc_binding, true,  0) ||
           zmk_behavior_queue_add(&event, msc_binding, false, 15);
#endif
}

static const struct behavior_driver_api behavior_ec_msc_driver_api = {
    .sensor_binding_accept_data = on_sensor_binding_accept_data,
    .sensor_binding_process     = on_sensor_binding_process,
};

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