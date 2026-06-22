/*
 * Copyright (c) 2026 Your Name
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_encoder_mouse_scroll

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>
#include <zmk/behavior.h>

#if !defined(CONFIG_ZMK_SPLIT) || defined(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
#include <zmk/hid.h>
#include <zmk/endpoints.h>
#include <dt-bindings/zmk/pointing.h>
#endif

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct behavior_ec_ms_data {
    int32_t remainder;
    int32_t triggers;
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

#if !defined(CONFIG_ZMK_SPLIT) || defined(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    struct behavior_ec_ms_data *data = dev->data;
    const struct sensor_value value = channel_data[0].value;

    data->remainder += value.val1;
    int trigger_degrees = 360 / sensor_config->triggers_per_rotation;
    int triggers = data->remainder / trigger_degrees;

    data->remainder %= trigger_degrees;
    data->triggers = triggers;

    //fordebug
    LOG_DBG("ec_ms accept: val1=%d, remainder=%d, trigger_degrees=%d, triggers=%d",
            value.val1, data->remainder, trigger_degrees, triggers);

#endif

    return 0;
}

static int behavior_ec_ms_process(struct zmk_behavior_binding *binding,
                                  struct zmk_behavior_binding_event event,
                                  enum behavior_sensor_binding_process_mode mode) {
    if (mode != BEHAVIOR_SENSOR_BINDING_PROCESS_MODE_TRIGGER) {
        return ZMK_BEHAVIOR_TRANSPARENT;
    }

#if !defined(CONFIG_ZMK_SPLIT) || defined(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    if (dev == NULL) {
        return -ENODEV;
    }
    struct behavior_ec_ms_data *data = dev->data;

    int32_t triggers = data->triggers;
    data->triggers = 0; 

    if (triggers == 0) {
        return ZMK_BEHAVIOR_TRANSPARENT;
    }

    uint32_t param = (triggers > 0) ? binding->param1 : binding->param2;

    int16_t h_wheel = MOVE_X_DECODE(param);
    int16_t wheel = MOVE_Y_DECODE(param);

    int abs_trig = (triggers > 0) ? triggers : -triggers;
    h_wheel *= abs_trig;
    wheel *= abs_trig;

    LOG_DBG("Encoder One-shot Scroll: triggers=%d, h_wheel=%d, wheel=%d", triggers, h_wheel, wheel);

    zmk_hid_mouse_scroll_set(h_wheel, wheel);
    zmk_endpoint_send_mouse_report();

    zmk_hid_mouse_scroll_set(0, 0);
    zmk_endpoint_send_mouse_report();

    return ZMK_BEHAVIOR_OPAQUE;
#else
    return ZMK_BEHAVIOR_TRANSPARENT;
#endif
}

static const struct behavior_driver_api behavior_encoder_mouse_scroll_driver_api = {
    .sensor_binding_accept_data = behavior_ec_ms_accept_data,
    .sensor_binding_process = behavior_ec_ms_process
};

#define EMS_INST(n)                                                                                \
    static struct behavior_ec_ms_data behavior_ec_ms_data_##n = { .remainder = 0, .triggers = 0 };  \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, &behavior_ec_ms_data_##n, NULL, POST_KERNEL,             \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                    \
                            &behavior_encoder_mouse_scroll_driver_api);

DT_INST_FOREACH_STATUS_OKAY(EMS_INST)