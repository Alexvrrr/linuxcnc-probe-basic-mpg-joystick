# Кастомный USB-пульт (MPG + аналоговый стик) для LinuxCNC 2.9.10 и Probe Basic

Готовый проект аппаратно-программного пульта ручного управления (Pendant / MPG) для станка с ЧПУ под управлением **LinuxCNC 2.9.10** (плата **Mesa 7i92**) и графического интерфейса **Probe Basic (QtPyVCP)**.

---

## 1. Возможности и архитектура

* **Контроллер пульта:** Arduino Leonardo / Pro Micro (ATmega32U4), работающий как стандартный USB HID-геймпад (`Joystick.h`).
* **Квадратурный MPG-энкодер:** Считывание фаз `A` и `B` по аппаратным прерываниям Arduino (`INT1` / `INT0`), буферизация и мгновенный сброс хвоста очереди при остановке вращения (исключает инерционный «доезд» осей).
* **Выбор активной оси (`X`, `Y`, `Z`):** Перемещение по шагам выполняется при удержании кнопки нужной оси и вращении энкодера (на время удержания аналоговый стик программно блокируется в центр).
* **Переключение шага (`Step`):** Циклический перебор `1.0` → `0.1` → `0.01` мм одной кнопкой с выводом текущего значения на LCD-индикатор в интерфейсе Probe Basic.
* **Непрерывные перемещения:**
  * 2-осевой аналоговый стик (`X`, `Y`) с мёртвой зоной и экспоненциальной кривой отклика (`joyhandle`).
  * Кнопки `Z+` и `Z-` для плавного перемещения по оси Z.
  * Регулировка скорости непрерывных перемещений штатным слайдером **Feed Override** в Probe Basic.
* **Работа в любых режимах:** Поддержка перемещений как до поиска домов (**Joint mode**, `joint.0..2`), так и после (**World mode**, `axis.x/y/z`).
* **Аварийный стоп:** Выделенная аппаратная кнопка `E-Stop`.

---

## 2. Схема распайки пинов Arduino (Leonardo / Pro Micro)

Все дискретные входы (кнопки и фазы энкодера) используют внутреннюю подтяжку к плюсу питания (`INPUT_PULLUP`) и замыкаются на общий провод (**GND**).

| Пин Arduino | Назначение в пульте | Тип сигнала / подключение | Кнопка / Ось в `Joystick.h` | Имя пина в LinuxCNC (`hal_input`) |
| :--- | :--- | :--- | :--- | :--- |
| **D2** (`INT1`) | Энкодер MPG — **Фаза A** | Аппаратное прерывание, замыкание на `GND` | Виртуальная кнопка `7` (`MPG+`) | `input.0.btn-tr` |
| **D3** (`INT0`) | Энкодер MPG — **Фаза B** | Аппаратное прерывание, замыкание на `GND` | Виртуальная кнопка `8` (`MPG-`) | `input.0.btn-tl2` |
| **D4** | Выбор оси **X** (`BTN_X`) | Кнопка без фиксации на `GND` | Кнопка `4` | `input.0.btn-y` |
| **D5** | Выбор оси **Y** (`BTN_Y`) | Кнопка без фиксации на `GND` | Кнопка `5` | `input.0.btn-z` |
| **D6** | Выбор оси **Z** (`BTN_Z`) | Кнопка без фиксации на `GND` | Кнопка `6` | `input.0.btn-tl` |
| **D7** | Непрерывный ход **Z+** (`BTN_ZP`) | Кнопка без фиксации на `GND` | Кнопка `2` | `input.0.btn-a` |
| **D8** | Аварийный стоп **E-Stop** (`BTN_ESTOP`) | Кнопка на `GND` | Кнопка `1` | `input.0.btn-b` |
| **D9** | Переключение шага **Step** (`BTN_STEP`) | Кнопка без фиксации на `GND` | Кнопка `0` | `input.0.btn-c` |
| **D14** (`MISO`) | Непрерывный ход **Z-** (`BTN_ZM`) | Кнопка без фиксации на `GND` | Кнопка `3` | `input.0.btn-x` |
| **A0** | Аналоговый стик — **Ось X** | Средний вывод потенциометра (`0..5V`) | Ось `X` (`0..1023`, инверсия в скетче) | `input.0.abs-x-position` |
| **A1** | Аналоговый стик — **Ось Y** | Средний вывод потенциометра (`0..5V`) | Ось `Y` (`0..1023`, инверсия в HAL) | `input.0.abs-y-position` |
| **VCC / 5V** | Питание периферии | Питание активного MPG и потенциометров стика | — | — |
| **GND** | Общий провод | Общая земля для всех кнопок, стика и MPG | — | — |

> **Примечания по электрической части:**
> * На плате **Arduino Pro Micro** пин `14` подписан как **`MISO`** (верхний правый угол платы).
> * При длине кабеля более 1.5–2 м используйте экранированный кабель (экран на `GND` со стороны Arduino) и керамические конденсаторы `0.1 мкФ` между `A0`/`A1` и `GND`.

---

## 3. Прошивка Arduino (`MPG_Joystick.ino`)

Для компиляции требуется библиотека [ArduinoJoystickLibrary](https://github.com/MHeironimus/ArduinoJoystickLibrary).

```cpp
#include <Joystick.h>

Joystick_ Joystick(
  JOYSTICK_DEFAULT_REPORT_ID,
  JOYSTICK_TYPE_GAMEPAD,
  9, 0,                  // 9 кнопок (индексы 0..8), 0 хатов
  true, true, false,     // Оси X и Y включены, Z выключена
  false, false, false,
  false, false, false,
  false, false
);

// --- Пины подключения ---
const int PIN_ENC_A  = 2;  // Фаза A энкодера (аппаратное прерывание INT1)
const int PIN_ENC_B  = 3;  // Фаза B энкодера (аппаратное прерывание INT0)
const int BTN_X      = 4;  // Выбор оси X
const int BTN_Y      = 5;  // Выбор оси Y
const int BTN_Z      = 6;  // Выбор оси Z
const int BTN_ZP     = 7;  // Непрерывное движение Z+
const int BTN_ZM     = 14; // Непрерывное движение Z-
const int BTN_STEP   = 9;  // Переключение шага (1 -> 0.1 -> 0.01)
const int BTN_ESTOP  = 8;  // Аварийный стоп (E-Stop)
const int ADC_X      = A0; // Аналоговый стик X
const int ADC_Y      = A1; // Аналоговый стик Y

// --- Виртуальные кнопки джойстика для импульсов MPG ---
const int BTN_MPG_PLUS  = 7; // В LinuxCNC: input.0.btn-tr (вращение +)
const int BTN_MPG_MINUS = 8; // В LinuxCNC: input.0.btn-tl2 (вращение -)

// --- Переменные квадратурного энкодера ---
volatile long encTicks = 0;
volatile uint8_t prevAB = 0;

const int8_t QUAD_TABLE[16] = {
   0, -1,  1,  0,
   1,  0,  0, -1,
  -1,  0,  0,  1,
   0,  1, -1,  0
};

const int TICKS_PER_DETENT = 4;

// --- Переменные формирования импульсов и мгновенной остановки ---
long sentDetents = 0;
long prevTicks = 0;
unsigned long lastTickTime = 0;
unsigned long pulseTimer = 0;

int pulseState = 0;
int activeMpgBtn = -1;

const unsigned long PULSE_MS = 2;         // 2 мс импульс + 2 мс пауза (до 250 щелчков/сек)
const unsigned long STOP_TIMEOUT_MS = 25; // Сброс хвоста очереди через 25 мс после остановки руки

// --- Антидребезг кнопок выбора оси ---
bool btnAxisState[3] = {false, false, false};
bool btnAxisLastRaw[3] = {false, false, false};
unsigned long btnAxisTimer[3] = {0, 0, 0};
const unsigned long DEBOUNCE_MS = 20;

void isrEncoder() {
  uint8_t curAB = (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);
  uint8_t idx = (prevAB << 2) | curAB;
  encTicks += QUAD_TABLE[idx];
  prevAB = curAB;
}

bool readDebounced(int pin, int idx) {
  bool raw = !digitalRead(pin);
  if (raw != btnAxisLastRaw[idx]) {
    btnAxisTimer[idx] = millis();
    btnAxisLastRaw[idx] = raw;
  }
  if ((millis() - btnAxisTimer[idx]) >= DEBOUNCE_MS) {
    btnAxisState[idx] = raw;
  }
  return btnAxisState[idx];
}

void setup() {
  pinMode(BTN_X,     INPUT_PULLUP);
  pinMode(BTN_Y,     INPUT_PULLUP);
  pinMode(BTN_Z,     INPUT_PULLUP);
  pinMode(BTN_ZP,    INPUT_PULLUP);
  pinMode(BTN_ZM,    INPUT_PULLUP);
  pinMode(BTN_STEP,  INPUT_PULLUP);
  pinMode(BTN_ESTOP, INPUT_PULLUP);

  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);

  prevAB = (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), isrEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_B), isrEncoder, CHANGE);

  Joystick.begin();
  Joystick.setXAxisRange(0, 1023);
  Joystick.setYAxisRange(0, 1023);
  Joystick.setXAxis(512);
  Joystick.setYAxis(512);
}

void loop() {
  unsigned long now = millis();

  bool btnX = readDebounced(BTN_X, 0);
  bool btnY = readDebounced(BTN_Y, 1);
  bool btnZ = readDebounced(BTN_Z, 2);

  if (btnX || btnY || btnZ) {
    Joystick.setXAxis(512);
    Joystick.setYAxis(512);
  } else {
    Joystick.setXAxis(1023 - analogRead(ADC_X));
    Joystick.setYAxis(analogRead(ADC_Y));
  }

  noInterrupts();
  long currentTicks = encTicks;
  interrupts();

  if (currentTicks != prevTicks) {
    lastTickTime = now;
    prevTicks = currentTicks;
  }

  long targetDetents = currentTicks / TICKS_PER_DETENT;

  if (pulseState == 0 && (now - lastTickTime > STOP_TIMEOUT_MS)) {
    sentDetents = targetDetents;
  }

  if (pulseState == 0) {
    if (targetDetents > sentDetents) {
      activeMpgBtn = BTN_MPG_PLUS;
      Joystick.setButton(activeMpgBtn, 1);
      sentDetents++;
      pulseTimer = now;
      pulseState = 1;
    } else if (targetDetents < sentDetents) {
      activeMpgBtn = BTN_MPG_MINUS;
      Joystick.setButton(activeMpgBtn, 1);
      sentDetents--;
      pulseTimer = now;
      pulseState = 1;
    }
  } else if (pulseState == 1) {
    if (now - pulseTimer >= PULSE_MS) {
      Joystick.setButton(activeMpgBtn, 0);
      pulseTimer = now;
      pulseState = 2;
    }
  } else if (pulseState == 2) {
    if (now - pulseTimer >= PULSE_MS) {
      pulseState = 0;
    }
  }

  Joystick.setButton(0, !digitalRead(BTN_STEP));
  Joystick.setButton(1, !digitalRead(BTN_ESTOP));
  Joystick.setButton(2, !digitalRead(BTN_ZP));
  Joystick.setButton(3, !digitalRead(BTN_ZM));
  Joystick.setButton(4, btnX);
  Joystick.setButton(5, btnY);
  Joystick.setButton(6, btnZ);

  delay(1);
}
```

---

## 4. Конфигурация LinuxCNC 2.9.10

### 4.1. Настройка прав доступа USB (`udev`)
Создайте правило `/etc/udev/rules.d/99-joystick.rules` (подставив ваши `idVendor` и `idProduct` из `cat /proc/bus/input/devices`):
```text
SUBSYSTEM=="input", ATTRS{idVendor}=="XXXX", ATTRS{idProduct}=="YYYY", MODE="0666"
```
Примените правила:
```bash
sudo udevadm control --reload-rules && sudo udevadm trigger
```

### 4.2. Компонент переключения шага (`mpg_joy.py`)
Поместите в папку конфигурации станка и сделайте исполняемым (`chmod +x mpg_joy.py`):

```python
#!/usr/bin/env python3
import hal
import time

h = hal.component("mpg_joy")

h.newpin("btn-step", hal.HAL_BIT, hal.HAL_IN)
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
```

### 4.3. Логика пульта (`MPG_Alex.hal`)

```hal
# 1. Загрузка драйвера джойстика и Python-компонента переключения шага
loadusr -W hal_input -KRAL Arduino
loadusr -W ./mpg_joy.py

# 2. Загрузка RT-компонентов (счётчик MPG, сглаживание стика и масштаб скорости)
loadrt updown names=mpg_rt_cnt
loadrt joyhandle count=2
loadrt scale names=joy_speed_scale

addf mpg_rt_cnt servo-thread
addf joyhandle.0 servo-thread
addf joyhandle.1 servo-thread
addf joy_speed_scale servo-thread

# 3. Настройка сглаживания аналогового стика X/Y (с инверсией оси Y)
setp joyhandle.0.deadband 0.1
setp joyhandle.0.power 2.0
setp joyhandle.0.scale 1.0

setp joyhandle.1.deadband 0.1
setp joyhandle.1.power 2.0
setp joyhandle.1.scale -1.0

# 4. Переключение шага (1 -> 0.1 -> 0.01) кнопкой Step
net btn-step mpg_joy.btn-step <= input.0.btn-c

# 5. Настройка MPG-энкодера (работает и до Homed, и после Homed; режим мгновенной остановки 1)
setp axis.x.jog-vel-mode 1
setp axis.y.jog-vel-mode 1
setp axis.z.jog-vel-mode 1
setp joint.0.jog-vel-mode 1
setp joint.1.jog-vel-mode 1
setp joint.2.jog-vel-mode 1

net mpg-a mpg_rt_cnt.countup   <= input.0.btn-tr
net mpg-b mpg_rt_cnt.countdown <= input.0.btn-tl2

net mpg-scale  mpg_joy.step-out => axis.x.jog-scale axis.y.jog-scale axis.z.jog-scale joint.0.jog-scale joint.1.jog-scale joint.2.jog-scale
net mpg-counts mpg_rt_cnt.count => axis.x.jog-counts axis.y.jog-counts axis.z.jog-counts joint.0.jog-counts joint.1.jog-counts joint.2.jog-counts

net btn-xsig axis.x.jog-enable joint.0.jog-enable <= input.0.btn-y
net btn-ysig axis.y.jog-enable joint.1.jog-enable <= input.0.btn-z
net btn-zsel axis.z.jog-enable joint.2.jog-enable <= input.0.btn-tl

# 6. Управление скоростью джойстика со слайдера Feed Override Probe Basic
setp joy_speed_scale.gain 3000.0
net feed-override-val joy_speed_scale.in <= halui.feed-override.value
net jog-speed <= joy_speed_scale.out
net jog-speed => halui.joint.jog-speed

# 7. Непрерывные перемещения (стик X/Y и кнопки Z+/Z-)
unlinkp halui.axis.x.analog
unlinkp halui.axis.y.analog
unlinkp halui.joint.0.analog
unlinkp halui.joint.1.analog
unlinkp halui.axis.z.plus
unlinkp halui.axis.z.minus
unlinkp halui.joint.2.plus
unlinkp halui.joint.2.minus

net joy-x-raw joyhandle.0.in <= input.0.abs-x-position
net joy-y-raw joyhandle.1.in <= input.0.abs-y-position
net joy-x     halui.axis.x.analog halui.joint.0.analog <= joyhandle.0.out
net joy-y     halui.axis.y.analog halui.joint.1.analog <= joyhandle.1.out

net btn-zplus  halui.axis.z.plus  halui.joint.2.plus  <= input.0.btn-a
net btn-zminus halui.axis.z.minus halui.joint.2.minus <= input.0.btn-x

# 8. Кнопка E-Stop (аварийный стоп)
unlinkp halui.estop.activate
net btn-estop halui.estop.activate <= input.0.btn-b
```

### 4.4. Настройки `.ini` и индикация в Probe Basic (`probe_basic_postgui.hal`)

В `.ini` файле станка:
```ini
[EMCMOT]
SERVO_PERIOD = 1000000

[HAL]
HALUI = halui
TWOPASS = on
HALFILE = основной_файл.hal
HALFILE = custom.hal
HALFILE = MPG_Alex.hal
POSTGUI_HALFILE = probe_basic_postgui.hal
```

В сайдбар интерфейса Probe Basic (через `editvcp`) добавлен виджет `HalLCDNumber` (`qtpyvcp.widgets.hal_widgets.hal_lcd`) с `objectName = lcd_mpg_step`, `digitCount = 4`, `smallDecimalPoint = True`, `segmentStyle = Flat`.

Подключение в конце `probe_basic_postgui.hal`:
```hal
# Вывод выбранного шага MPG в LCD-индикатор сайдбара
net mpg-scale => qtpyvcp.lcd-mpg-step.float-value
```
