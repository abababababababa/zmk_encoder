zephyr_library()

zephyr_library_sources(
  src/behavior_sensor_adapter.c
)

target_sources_ifdef(CONFIG_ZMK_BEHAVIOR_SENSOR_ADAPTER app PRIVATE
  src/behavior_sensor_adapter.c
)