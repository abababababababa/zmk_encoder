/*
 * Copyright (c) 2024 ec_msc contributors
 * SPDX-License-Identifier: MIT
 *
 * behavior_ec_msc.c — EC11 encoder scroll behavior for ZMK main (Zephyr 4.1)
 *
 * Central (CONFIG_ZMK_POINTING=y && SPLIT_ROLE_CENTRAL, or non-split):
 *   accept_data resolves direction → process queues &msc press+release.
 *
 * Peripheral (CONFIG_ZMK_POINTING=n):
 *   accept_data runs; process is a no-op.
 *   DEVICE_DT_GET(&msc) is never referenced so no linker error.
 *
 * Keymap usage:
 *   ec_msc: ec_msc {
 *       compatible = "zmk,behavior-ec-msc";
 *       #sensor-binding-cells = <2>;
 *       msc-behavior = <&msc>;
 *   };
 *   sensor-bindings = <&ec_msc U D>;
 */

#define DT_DRV_COMPAT zmk_behavior_ec_msc

#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zmk/sensors.h>

/* Only pull in behavior_queue + pointing defines on Central */
#if IS_ENABLED(CONFIG_ZMK_POINTING) && \
    (IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL) || !IS_ENABLED(CONFIG_ZMK_SPLIT))
#include <zmk/behavior_queue.h>
#include <dt-bindings/zmk/pointing.h>
#define EC_MSC_HAS_QUEUE 1
#endif

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define EC_MSC_U 0
#define EC_MSC_D 1
#define EC_MSC_L 2
#define EC_MSC_R 3

struct behavior_ec_msc_data {
    uint8_t direction;
    bool    pending;
};

struct behavior_ec_msc_config {
#if defined(EC_MSC_HAS_QUEUE)
    const struct device *msc_dev;
#endif
};

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
    LOG_DBG("ec_msc accept: inc=%d param1=%d param2=%d",
            inc, (int)binding->param1, (int)binding->param2);

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
#if defined(EC_MSC_HAS_QUEUE)
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    struct behavior_ec_msc_data *data = dev->data;
    const struct behavior_ec_msc_config *cfg = dev->config;

    LOG_DBG("ec_msc process: pending=%d mode=%d", data->pending, (int)mode);

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

    LOG_DBG("ec_msc queuing via %s param=0x%08x", cfg->msc_dev->name, param);

    struct zmk_behavior_binding msc_binding = {
        .behavior_dev = cfg->msc_dev->name,
        .param1       = param,
        .param2       = 0,
    };

    zmk_behavior_queue_add(&event, msc_binding, true,  0);
    zmk_behavior_queue_add(&event, msc_binding, false, 0);
#endif

    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_ec_msc_driver_api = {
    .sensor_binding_accept_data = on_sensor_binding_accept_data,
    .sensor_binding_process     = on_sensor_binding_process,
};

/* Central: resolve &msc device pointer from DTS phandle */
#if defined(EC_MSC_HAS_QUEUE)
#define EC_MSC_INST(n)                                                              \
    static struct behavior_ec_msc_data behavior_ec_msc_data_##n = {};              \
    static const struct behavior_ec_msc_config behavior_ec_msc_config_##n = {      \
        .msc_dev = DEVICE_DT_GET(DT_INST_PHANDLE(n, msc_behavior)),                \
    };                                                                              \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL,                                         \
                            &behavior_ec_msc_data_##n,                             \
                            &behavior_ec_msc_config_##n,                            \
                            POST_KERNEL,                                            \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                    \
                            &behavior_ec_msc_driver_api);
#else
/* Peripheral: no msc_dev */
#define EC_MSC_INST(n)                                                              \
    static struct behavior_ec_msc_data behavior_ec_msc_data_##n = {};              \
    static const struct behavior_ec_msc_config behavior_ec_msc_config_##n = {};    \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL,                                         \
                            &behavior_ec_msc_data_##n,                             \
                            &behavior_ec_msc_config_##n,                            \
                            POST_KERNEL,                                            \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                    \
                            &behavior_ec_msc_driver_api);
#endif

DT_INST_FOREACH_STATUS_OKAY(EC_MSC_INST)