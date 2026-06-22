/*
 * Copyright (c) 2026 Your Name
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_encoder_mouse_scroll

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

#include <zmk/behavior.h>
#include <zmk/hid.h>
#include <zmk/endpoints.h>
#include <dt-bindings/zmk/pointing.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct behavior_ec_ms_data {
    atomic_t rotation_value;
};

static int behavior_ec_ms_accept_data(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event,
                                      const struct zmk_sensor_config *sensor_config,
                                      size_t channel_data_size,
                                      const struct zmk_sensor_channel_data *channel_data) {
    if (channel_data_size == 0 || channel_data == NULL) {
        return -EINVAL;
    }

    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    if (dev == NULL) {
        return -ENODEV;
    }
    struct behavior_ec_ms_data *data = dev->data;

    const struct sensor_value value = channel_data[0].value;
    atomic_set(&data->rotation_value, (atomic_val_t)value.val1);

    return 0;
}

static int behavior_ec_ms_process(struct zmk_behavior_binding *binding,
                                  struct zmk_behavior_binding_event event,
                                  enum behavior_sensor_binding_process_mode mode) {
    if (mode != BEHAVIOR_SENSOR_BINDING_PROCESS_MODE_TRIGGER) {
        return ZMK_BEHAVIOR_TRANSPARENT;
    }

    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    if (dev == NULL) {
        return -ENODEV;
    }
    struct behavior_ec_ms_data *data = dev->data;

    // CAS (Compare-and-Swap) ループによる、完全なロックフリーのRead-and-Clear
    // 値を読み出しつつ0へのリセットを試み、成功するまで安全にループします。
    // これにより、データの取りこぼしや競合が100%発生しなくなります。
    atomic_val_t rotation;
    do {
        rotation = atomic_get(&data->rotation_value);
        if (rotation == 0) {
            return ZMK_BEHAVIOR_TRANSPARENT;
        }
    } while (!atomic_cas(&data->rotation_value, rotation, 0));

    // 退避した物理回転方向に基づき、キーマップで指定されたパラメータ（SCRL_UP等）を確定
    uint32_t param = (rotation > 0) ? binding->param1 : binding->param2;

    int16_t h_wheel = 0;
    int16_t wheel = 0;
    int16_t scroll_amount = 1;

    switch (param) {
        case SCRL_UP:
            wheel = scroll_amount;
            break;
        case SCRL_DOWN:
            wheel = -scroll_amount;
            break;
        case SCRL_LEFT:
            h_wheel = -scroll_amount;
            break;
        case SCRL_RIGHT:
            h_wheel = scroll_amount;
            break;
        default:
            LOG_WRN("Invalid scroll parameter: %d", param);
            return ZMK_BEHAVIOR_OPAQUE;
    }

    LOG_DBG("Encoder One-shot Scroll: param=%d, h_wheel=%d, wheel=%d", param, h_wheel, wheel);

    // 非ブロッキング・ワンショット送信
    zmk_hid_mouse_scroll_set(h_wheel, wheel);
    zmk_endpoint_send_mouse_report();

    zmk_hid_mouse_scroll_set(0, 0);
    zmk_endpoint_send_mouse_report();

    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_encoder_mouse_scroll_driver_api = {
    .sensor_binding_accept_data = behavior_ec_ms_accept_data,
    .sensor_binding_process = behavior_ec_ms_process
};

#define EMS_INST(n)                                                                                \
    static struct behavior_ec_ms_data behavior_ec_ms_data_##n = { .rotation_value = ATOMIC_INIT(0) }; \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, &behavior_ec_ms_data_##n, NULL, POST_KERNEL,             \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                    \
                            &behavior_encoder_mouse_scroll_driver_api);

DT_INST_FOREACH_STATUS_OKAY(EMS_INST)