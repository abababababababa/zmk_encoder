zephyr_library()

zephyr_library_sources_ifdef(CONFIG_ZMK_BEHAVIOR_SENSOR_ADAPTER
  src/behavior_sensor_adapter.c
)