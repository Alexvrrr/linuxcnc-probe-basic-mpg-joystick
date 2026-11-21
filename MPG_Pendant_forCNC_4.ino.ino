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

// Таблица переходов Грея для квадратурного декодирования
const int8_t QUAD_TABLE[16] = {
   0, -1,  1,  0,
   1,  0,  0, -1,
  -1,  0,  0,  1,
   0,  1, -1,  0
};

// Количество квадратурных переходов на 1 физический щелчок (обычно 4; если пропускает через один — ставьте 2)
const int TICKS_PER_DETENT = 4;

// --- Переменные формирования импульсов и плавной остановки ---
long sentDetents = 0;
long prevTicks = 0;
unsigned long lastTickTime = 0;
unsigned long pulseTimer = 0;

int pulseState = 0; // 0 = ожидание, 1 = импульс нажатия, 2 = пауза между импульсами
int activeMpgBtn = -1;

const unsigned long PULSE_MS = 2;         // 2 мс импульс + 2 мс пауза (до 250 щелчков/сек)
const unsigned long STOP_TIMEOUT_MS = 25; // Сброс хвоста очереди через 25 мс после остановки руки

// --- Переменные антидребезга кнопок выбора оси ---
bool btnAxisState[3] = {false, false, false};
bool btnAxisLastRaw[3] = {false, false, false};
unsigned long btnAxisTimer[3] = {0, 0, 0};
const unsigned long DEBOUNCE_MS = 20;

// Обработчик прерывания энкодера
void isrEncoder() {
  uint8_t curAB = (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);
  uint8_t idx = (prevAB << 2) | curAB;
  encTicks += QUAD_TABLE[idx];
  prevAB = curAB;
}

// Функция антидребезга для кнопок X, Y, Z
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

  // 1. Чтение кнопок выбора оси с антидребезгом
  bool btnX = readDebounced(BTN_X, 0);
  bool btnY = readDebounced(BTN_Y, 1);
  bool btnZ = readDebounced(BTN_Z, 2);

  // 2. Аналоговый стик X/Y (блокируется в центр при удержании кнопок выбора оси X/Y/Z)
  if (btnX || btnY || btnZ) {
    Joystick.setXAxis(512);
    Joystick.setYAxis(512);
  } else {
    Joystick.setXAxis(1023 - analogRead(ADC_X)); // инверсия оси X
    Joystick.setYAxis(analogRead(ADC_Y));
  }

  // 3. Чтение счётчика энкодера из прерываний
  noInterrupts();
  long currentTicks = encTicks;
  interrupts();

  if (currentTicks != prevTicks) {
    lastTickTime = now;
    prevTicks = currentTicks;
  }

  long targetDetents = currentTicks / TICKS_PER_DETENT;

  // Сброс невыполненного хвоста очереди, если маховичок остановился (> 25 мс без новых импульсов)
  if (pulseState == 0 && (now - lastTickTime > STOP_TIMEOUT_MS)) {
    sentDetents = targetDetents;
  }

  // 4. Генерация неперекрывающихся импульсов для USB HID
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

  // 5. Остальные кнопки управления
  Joystick.setButton(0, !digitalRead(BTN_STEP));
  Joystick.setButton(1, !digitalRead(BTN_ESTOP));
  Joystick.setButton(2, !digitalRead(BTN_ZP));
  Joystick.setButton(3, !digitalRead(BTN_ZM));
  Joystick.setButton(4, btnX);
  Joystick.setButton(5, btnY);
  Joystick.setButton(6, btnZ);

  delay(1);
}
