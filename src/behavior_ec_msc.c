/*
 * Copyright (c) 2024 ec_msc contributors
 * SPDX-License-Identifier: MIT
 *
 * behavior_ec_msc.c
 */

#define DT_DRV_COMPAT zmk_behavior_ec_msc

#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/input/input.h> /* ← 追加: Zephyrの標準入力サブシステム */
#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zmk/sensors.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/* ── Direction tokens (include/behaviors/ec_msc.h と一致させる) ──────── */
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

/* ── Phase 1: accept_data — センサーの回転方向を受け取る ──────── */
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

    /* 回転方向（正か負か）によって、DTSで指定されたパラメーターを選択 */
    data->direction = (inc > 0) ? (uint8_t)binding->param1
                                : (uint8_t)binding->param2;
    data->pending   = true;
    return 0;
}

/* ── Phase 2: process — スクロールイベントを直接発火する ────── */
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

#if IS_ENABLED(CONFIG_ZMK_SPLIT) && !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    /* Peripheral側は入力データをCentralに送るだけで自身では処理しない */
    return ZMK_BEHAVIOR_OPAQUE;
#else
    /* Central側、または単体キーボードの場合：1ノッチ分のスクロールを直接発火 */
    int32_t val = 1;
    uint16_t code = INPUT_REL_WHEEL; /* デフォルトは縦ホイール */

    switch (data->direction) {
        case EC_MSC_U:
            code = INPUT_REL_WHEEL;
            val = 1;   /* 上 */
            break;
        case EC_MSC_D:
            code = INPUT_REL_WHEEL;
            val = -1;  /* 下 */
            break;
        case EC_MSC_R:
            code = INPUT_REL_HWHEEL;
            val = 1;   /* 右 */
            break;
        case EC_MSC_L:
            code = INPUT_REL_HWHEEL;
            val = -1;  /* 左 */
            break;
    }

    /* Zephyrのinputサブシステムに直接イベントを投げ込む */
    input_report_rel(dev, code, val, true, K_NO_WAIT);

    return ZMK_BEHAVIOR_OPAQUE;
#endif
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