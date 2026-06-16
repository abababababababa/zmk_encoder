/*
 * Simple sensor adapter behavior for ZMK
 * - accumulates dx/dy
 * - exposes no HID output
 * - intended as preprocessing layer for other behaviors
 */

#define DT_DRV_COMPAT zmk_behavior_sensor_adapter

#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <drivers/behavior.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

struct sensor_adapter_config {
    uint8_t dummy; // reserved for future config
};

struct sensor_adapter_data {
    int32_t x;
    int32_t y;
};

/* init */
static int sensor_adapter_init(const struct device *dev) {
    struct sensor_adapter_data *data = dev->data;

    data->x = 0;
    data->y = 0;

    return 0;
}

/*
 * This is the IMPORTANT hook
 * ZMK sensor behaviors call this path
 */
static int sensor_adapter_adjust_speed(
    const struct device *dev,
    int16_t dx,
    int16_t dy)
{
    struct sensor_adapter_data *data = dev->data;

    data->x += dx;
    data->y += dy;

    LOG_DBG("adapter accum: x=%d y=%d", data->x, data->y);

    return 0;
}

/*
 * Optional: reset on release / lifecycle hook
 */
static int sensor_adapter_process(
    const struct device *dev,
    struct zmk_behavior_binding *binding,
    struct zmk_behavior_binding_event event,
    enum behavior_sensor_binding_process_mode mode)
{
    ARG_UNUSED(binding);
    ARG_UNUSED(event);

    struct sensor_adapter_data *data = dev->data;

    if (mode != BEHAVIOR_SENSOR_BINDING_PROCESS_MODE_TRIGGER) {
        data->x = 0;
        data->y = 0;
        return ZMK_BEHAVIOR_TRANSPARENT;
    }

    /* nothing emitted */
    return ZMK_BEHAVIOR_OPAQUE;
}

/* ZMK behavior driver API */
static const struct behavior_driver_api sensor_adapter_driver_api = {
    .sensor_binding_adjust_speed = sensor_adapter_adjust_speed,
    .sensor_binding_process = sensor_adapter_process,
};

/* device init */
#define SENSOR_ADAPTER_INST(n)                                  \
    static struct sensor_adapter_data data_##n;                 \
    static const struct sensor_adapter_config config_##n = {};  \
                                                                \
    BEHAVIOR_DT_INST_DEFINE(                                    \
        n,                                                      \
        sensor_adapter_init, NULL,                              \
        &data_##n, &config_##n,                                 \
        POST_KERNEL,                                            \
        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                    \
        &sensor_adapter_driver_api);

DT_INST_FOREACH_STATUS_OKAY(SENSOR_ADAPTER_INST)