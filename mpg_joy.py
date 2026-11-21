#!/usr/bin/env python3
import hal
import time

h = hal.component("mpg_joy")

# Вход: кнопка переключения шага
h.newpin("btn-step", hal.HAL_BIT, hal.HAL_IN)

# Выходы: значение шага и индикаторы для Probe Basic
h.newpin("step-out", hal.HAL_FLOAT, hal.HAL_OUT)
h.newpin("led-1", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("led-01", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("led-001", hal.HAL_BIT, hal.HAL_OUT)

h.ready()

steps = [1.0, 0.1, 0.01]
step_idx = 0
prev_step = False

try:
    while True:
        cur_step = h["btn-step"]
        if cur_step and not prev_step:
            step_idx = (step_idx + 1) % 3
        prev_step = cur_step

        h["step-out"] = steps[step_idx]
        h["led-1"] = (step_idx == 0)
        h["led-01"] = (step_idx == 1)
        h["led-001"] = (step_idx == 2)

        time.sleep(0.01)
except KeyboardInterrupt:
    raise SystemExit
