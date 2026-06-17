/*
 * Copyright (c) 2024 ec_msc contributors
 * SPDX-License-Identifier: MIT
 *
 * behavior_ec_msc.c
 *
<<<<<<< HEAD
 * EC11 encoder scroll behavior for ZMK main (Zephyr 4.1).
=======
 * EC11 encoder scroll behavior for ZMK main (Zephyr 4.1).
 *
 * Instead of calling input_report_rel() directly (which requires a valid
 * virtual device pointer that is internal to ZMK's pointing subsystem),
 * this behavior invokes the built-in &msc behavior via zmk_behavior_queue_add,
 * exactly like behavior_sensor_rotate_common does for &kp.
 *
 * This means:
 *   - Press and Release are both queued atomically — no "stuck" scroll state.
 *   - All of ZMK's HID plumbing (BLE, USB, split) is handled correctly.
 *   - No dependency on internal pointing device pointers.
 *
 * Keymap usage:
 *   sensor-bindings = <&ec_msc U D>;   // CW=Up,    CCW=Down
 *   sensor-bindings = <&ec_msc R L>;   // CW=Right, CCW=Left
 *
 * Direction tokens defined in include/behaviors/ec_msc.h:
 *   U=0, D=1, L=2, R=3
>>>>>>> 2d20d25b3cc72b12819e3f12efda2d5def8ebeae
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

/* ── Direction tokens (must match include/behaviors/ec_msc.h) ──────── */
#define EC_MSC_U 0
#define EC_MSC_D 1
#define EC_MSC_L 2
#define EC_MSC_R 3

/*
 * Map direction token to the &msc parameter value.
 * SCRL_UP / SCRL_DOWN / SCRL_LEFT / SCRL_RIGHT are defined in
 * dt-bindings/zmk/pointing.h and encode the velocity as a uint32.
 */
static uint32_t direction_to_msc_param(uint8_t direction)
{
    switch (direction) {
    case EC_MSC_U: return SCRL_UP;
    case EC_MSC_D: return SCRL_DOWN;
    case EC_MSC_L: return SCRL_LEFT;
    case EC_MSC_R: return SCRL_RIGHT;
    default:       return SCRL_DOWN; /* fallback */
    }
}

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

/* ── Phase 2: process — queue msc press+release via behavior_queue ────── */
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

    /*
     * Build an &msc binding and queue press + immediate release.
     * zmk_behavior_queue_add() enqueues the press, then after wait_ms=0
     * enqueues the release — same pattern as sensor_rotate_common.
     */
    struct zmk_behavior_binding msc_binding = {
        .behavior_dev = DEVICE_DT_NAME(DT_NODELABEL(msc)),  /* matches the DTS label of &msc */
        .param1       = param,
        .param2       = 0,
    };

/* ↓↓↓ ここから修正箇所 ↓↓↓ */
#if IS_ENABLED(CONFIG_ZMK_SPLIT) && !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    /* Peripheral側は入力データをCentralに送るだけで、Behavior自体は実行しない */
    return ZMK_BEHAVIOR_OPAQUE;
#else
    /* Central側、または単体キーボードの場合はキューに積む */
    /* press, 0 ms hold, then release */
    return zmk_behavior_queue_add(&event, msc_binding, true,  0) ||
           zmk_behavior_queue_add(&event, msc_binding, false, 0);
#endif
/* ↑↑↑ ここまで修正箇所 ↑↑↑ */
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