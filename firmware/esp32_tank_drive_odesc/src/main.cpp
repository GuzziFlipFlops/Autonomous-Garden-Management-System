#include <Arduino.h>

static const uint16_t PWM_MAX_DUTY = 4095;
static const uint32_t DEFAULT_PWM_HZ = 1000;
static const uint32_t MIN_PWM_HZ = 1;
static const uint32_t MAX_PWM_HZ = 50000;
static const uint16_t ADC_MAX_CHANNELS = 4;
static const uint16_t ADC_DEFAULT_MS = 100;
static const uint16_t ADC_MIN_MS = 50;
static const uint16_t ADC_MAX_MS = 5000;
static const uint32_t ADC_MV_REF = 3300;
static const uint16_t DRIVE_RAMP_MS = 300;
static const uint16_t DRIVE_STEP_MS = 25;

enum PinModeState : uint8_t {
  MODE_IDLE,
  MODE_DIGITAL,
  MODE_PWM,
  MODE_ADC,
};

struct PinEntry {
  const char *name;
  uint32_t pin;
  bool adcCapable;
};

static PinEntry pins[] = {
    {"PA0", PA0, true},   {"PA1", PA1, true},   {"PA2", PA2, true},
    {"PA3", PA3, true},   {"PA4", PA4, true},   {"PA5", PA5, true},
    {"PA6", PA6, true},   {"PA7", PA7, true},   {"PA8", PA8, false},
    {"PA11", PA11, false}, {"PA12", PA12, false}, {"PB0", PB0, true},
    {"PB1", PB1, true},   {"PB5", PB5, false},  {"PB6", PB6, false},
    {"PB7", PB7, false},  {"PB8", PB8, false},  {"PB9", PB9, false},
    {"PB10", PB10, false}, {"PB11", PB11, false}, {"PB12", PB12, false},
    {"PB13", PB13, false}, {"PB14", PB14, false}, {"PB15", PB15, false},
    {"PC13", PC13, false}, {"PC14", PC14, false}, {"PC15", PC15, false},
};

static const size_t PIN_COUNT = sizeof(pins) / sizeof(pins[0]);
static String inputLine;
static PinModeState modeState[PIN_COUNT];
static uint16_t outputValue[PIN_COUNT];
static uint32_t pwmFrequency[PIN_COUNT];
static bool adcEnabled[PIN_COUNT];
static uint16_t adcIntervalMs[PIN_COUNT];
static uint32_t adcNextDueMs[PIN_COUNT];
static uint8_t activeAdcChannels = 0;
static int8_t driveCurrentLeft = 0;
static int8_t driveCurrentRight = 0;
static int8_t driveTargetLeft = 0;
static int8_t driveTargetRight = 0;
static uint32_t driveNextStepMs = 0;

struct MotorPins {
  const char *name;
  const char *forward;
  const char *reverse;
};

static const MotorPins tankMotors[] = {
    {"front_left", "PA6", "PA7"},
    {"back_left", "PB13", "PA8"},
    {"front_right", "PB14", "PB15"},
    {"back_right", "PA3", "PB0"},
};

static const size_t TANK_MOTOR_COUNT = sizeof(tankMotors) / sizeof(tankMotors[0]);

static int findPinIndex(String name) {
  name.trim();
  name.toUpperCase();
  for (size_t i = 0; i < PIN_COUNT; i++) {
    if (name == pins[i].name) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

static bool hasPwm(size_t index) {
  return digitalPinHasPWM(pins[index].pin);
}

static bool hasAdc(size_t index) {
  return pins[index].adcCapable;
}

static void sendError(const char *reason) {
  Serial1.print("ERR ");
  Serial1.println(reason);
}

static void sendOkPinValue(const char *command, size_t index, uint32_t value) {
  Serial1.print("OK ");
  Serial1.print(command);
  Serial1.print(' ');
  Serial1.print(pins[index].name);
  Serial1.print(' ');
  Serial1.println(value);
}

static String nextToken(String &line) {
  line.trim();
  if (line.length() == 0) {
    return "";
  }

  int split = line.indexOf(' ');
  if (split < 0) {
    String token = line;
    line = "";
    return token;
  }

  String token = line.substring(0, split);
  line = line.substring(split + 1);
  return token;
}

static bool parseUint(const String &token, uint32_t &value) {
  if (token.length() == 0) {
    return false;
  }

  uint32_t parsed = 0;
  for (size_t i = 0; i < token.length(); i++) {
    char c = token.charAt(i);
    if (c < '0' || c > '9') {
      return false;
    }
    parsed = (parsed * 10) + static_cast<uint32_t>(c - '0');
  }

  value = parsed;
  return true;
}

static bool parseIntValue(const String &token, int32_t &value) {
  if (token.length() == 0) {
    return false;
  }

  int sign = 1;
  size_t start = 0;
  if (token.charAt(0) == '-') {
    sign = -1;
    start = 1;
  }
  if (start >= token.length()) {
    return false;
  }

  int32_t parsed = 0;
  for (size_t i = start; i < token.length(); i++) {
    char c = token.charAt(i);
    if (c < '0' || c > '9') {
      return false;
    }
    parsed = (parsed * 10) + static_cast<int32_t>(c - '0');
  }

  value = parsed * sign;
  return true;
}

static int8_t clampPercent(int32_t value) {
  if (value < -100) {
    return -100;
  }
  if (value > 100) {
    return 100;
  }
  return static_cast<int8_t>(value);
}

static void stopAdc(size_t index) {
  if (adcEnabled[index]) {
    adcEnabled[index] = false;
    activeAdcChannels--;
  }
}

static void prepareOutput(size_t index) {
  stopAdc(index);
  if (modeState[index] != MODE_DIGITAL) {
    pinMode(pins[index].pin, OUTPUT);
  }
  modeState[index] = MODE_DIGITAL;
}

static void setDigital(size_t index, uint8_t value) {
  prepareOutput(index);
  outputValue[index] = value ? 1 : 0;
  digitalWrite(pins[index].pin, value ? HIGH : LOW);
}

static bool setPwm(size_t index, uint16_t duty, uint32_t hz) {
  if (!hasPwm(index)) {
    sendError("NO_PWM");
    return false;
  }
  if (hz < MIN_PWM_HZ || hz > MAX_PWM_HZ) {
    sendError("BAD_FREQ");
    return false;
  }

  stopAdc(index);
  analogWriteFrequency(hz);
  analogWrite(pins[index].pin, duty);
  modeState[index] = MODE_PWM;
  outputValue[index] = duty;
  pwmFrequency[index] = hz;
  return true;
}

static void setOutputPercent(size_t index, uint8_t percent) {
  percent = percent > 100 ? 100 : percent;
  if (percent == 0) {
    setDigital(index, 0);
  } else if (percent >= 100) {
    setDigital(index, 1);
  } else {
    uint16_t duty = static_cast<uint16_t>((static_cast<uint32_t>(percent) * PWM_MAX_DUTY) / 100);
    if (duty == 0) {
      duty = 1;
    }
    setPwm(index, duty, DEFAULT_PWM_HZ);
  }
}

static void applyTankMotor(const MotorPins &motor, int8_t speed) {
  int forwardIndex = findPinIndex(String(motor.forward));
  int reverseIndex = findPinIndex(String(motor.reverse));
  if (forwardIndex < 0 || reverseIndex < 0) {
    return;
  }

  if (speed == 0) {
    setDigital(forwardIndex, 0);
    setDigital(reverseIndex, 0);
    return;
  }

  uint8_t magnitude = static_cast<uint8_t>(abs(speed));
  if (speed > 0) {
    setDigital(reverseIndex, 0);
    setOutputPercent(forwardIndex, magnitude);
  } else {
    setDigital(forwardIndex, 0);
    setOutputPercent(reverseIndex, magnitude);
  }
}

static int findTankMotorIndex(String name) {
  name.trim();
  name.toLowerCase();
  for (size_t i = 0; i < TANK_MOTOR_COUNT; i++) {
    if (name == tankMotors[i].name) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

static void applyTankDrive(int8_t left, int8_t right) {
  applyTankMotor(tankMotors[0], left);
  applyTankMotor(tankMotors[1], left);
  applyTankMotor(tankMotors[2], right);
  applyTankMotor(tankMotors[3], right);
}

static int8_t approachDriveValue(int8_t current, int8_t target) {
  if (current == target) {
    return current;
  }

  int8_t step = static_cast<int8_t>((100 * DRIVE_STEP_MS) / DRIVE_RAMP_MS);
  if (step < 1) {
    step = 1;
  }

  if (current < target) {
    int16_t next = current + step;
    return next > target ? target : static_cast<int8_t>(next);
  }

  int16_t next = current - step;
  return next < target ? target : static_cast<int8_t>(next);
}

static void stopTankDrive() {
  driveCurrentLeft = 0;
  driveCurrentRight = 0;
  driveTargetLeft = 0;
  driveTargetRight = 0;
  applyTankDrive(0, 0);
}

static void setTankTarget(int8_t left, int8_t right) {
  driveTargetLeft = left;
  driveTargetRight = right;
}

static uint16_t readAdcRaw(size_t index) {
  if (modeState[index] != MODE_ADC) {
    pinMode(pins[index].pin, INPUT_ANALOG);
    modeState[index] = MODE_ADC;
  }
  return static_cast<uint16_t>(analogRead(pins[index].pin));
}

static uint16_t rawToMv(uint16_t raw) {
  return static_cast<uint16_t>((static_cast<uint32_t>(raw) * ADC_MV_REF) / PWM_MAX_DUTY);
}

static void sendAdcSample(size_t index) {
  uint16_t raw = readAdcRaw(index);
  Serial1.print("ADC ");
  Serial1.print(pins[index].name);
  Serial1.print(' ');
  Serial1.print(raw);
  Serial1.print(' ');
  Serial1.println(rawToMv(raw));
}

static bool configureAdc(size_t index, uint16_t intervalMs) {
  if (!hasAdc(index)) {
    sendError("NO_ADC");
    return false;
  }
  if (modeState[index] == MODE_PWM || modeState[index] == MODE_DIGITAL) {
    digitalWrite(pins[index].pin, LOW);
  }
  if (intervalMs < ADC_MIN_MS || intervalMs > ADC_MAX_MS) {
    sendError("BAD_INTERVAL");
    return false;
  }
  if (!adcEnabled[index] && activeAdcChannels >= ADC_MAX_CHANNELS) {
    sendError("ADC_FULL");
    return false;
  }

  if (!adcEnabled[index]) {
    adcEnabled[index] = true;
    activeAdcChannels++;
  }

  adcIntervalMs[index] = intervalMs;
  adcNextDueMs[index] = millis() + intervalMs;
  pinMode(pins[index].pin, INPUT_ANALOG);
  modeState[index] = MODE_ADC;

  Serial1.print("OK ADCON ");
  Serial1.print(pins[index].name);
  Serial1.print(' ');
  Serial1.println(intervalMs);
  return true;
}

static void printPins() {
  Serial1.print("PINS");
  for (size_t i = 0; i < PIN_COUNT; i++) {
    Serial1.print(' ');
    Serial1.print(pins[i].name);
    Serial1.print(':');
    Serial1.print('D');
    if (hasPwm(i)) {
      Serial1.print('P');
    }
    if (hasAdc(i)) {
      Serial1.print('A');
    }
  }
  Serial1.println();
}

static void printAdcList() {
  Serial1.print("ADCS");
  for (size_t i = 0; i < PIN_COUNT; i++) {
    if (adcEnabled[i]) {
      Serial1.print(' ');
      Serial1.print(pins[i].name);
      Serial1.print(':');
      Serial1.print(adcIntervalMs[i]);
    }
  }
  Serial1.println();
}

static void resetAll() {
  for (size_t i = 0; i < PIN_COUNT; i++) {
    stopAdc(i);
    if (modeState[i] == MODE_DIGITAL || modeState[i] == MODE_PWM) {
      analogWriteFrequency(DEFAULT_PWM_HZ);
      analogWrite(pins[i].pin, 0);
      pinMode(pins[i].pin, OUTPUT);
      digitalWrite(pins[i].pin, LOW);
    }
    modeState[i] = MODE_IDLE;
    outputValue[i] = 0;
    pwmFrequency[i] = DEFAULT_PWM_HZ;
    adcIntervalMs[i] = ADC_DEFAULT_MS;
    adcNextDueMs[i] = 0;
  }
  activeAdcChannels = 0;
  Serial1.println("OK RESET");
}

static void handleCommand(String line) {
  line.trim();
  if (line.length() == 0) {
    return;
  }

  String command = nextToken(line);
  command.toUpperCase();

  if (command == "PING") {
    Serial1.println("OK PONG");
    return;
  }
  if (command == "CAPS") {
    Serial1.println("CAPS PWM_RES 12 PWM_MAX 4095 PWM_HZ 1-50000 ADC_RES 12 ADC_MAX 4 ADC_MS 50-5000 DRIVE -100..100");
    return;
  }
  if (command == "PINS") {
    printPins();
    return;
  }
  if (command == "ADCLIST") {
    printAdcList();
    return;
  }
  if (command == "RESET") {
    resetAll();
    stopTankDrive();
    return;
  }

  if (command == "STOP") {
    stopTankDrive();
    Serial1.println("OK STOP");
    return;
  }

  if (command == "DRIVE") {
    int32_t left = 0;
    int32_t right = 0;
    if (!parseIntValue(nextToken(line), left) || !parseIntValue(nextToken(line), right)) {
      sendError("BAD_DRIVE");
      return;
    }
    setTankTarget(clampPercent(left), clampPercent(right));
    Serial1.print("OK DRIVE ");
    Serial1.print(driveTargetLeft);
    Serial1.print(' ');
    Serial1.println(driveTargetRight);
    return;
  }

  if (command == "MOTOR") {
    String motorName = nextToken(line);
    int motorIndex = findTankMotorIndex(motorName);
    int32_t speed = 0;
    if (motorIndex < 0 || !parseIntValue(nextToken(line), speed)) {
      sendError("BAD_MOTOR");
      return;
    }
    driveTargetLeft = 0;
    driveTargetRight = 0;
    driveCurrentLeft = 0;
    driveCurrentRight = 0;
    applyTankDrive(0, 0);
    applyTankMotor(tankMotors[motorIndex], clampPercent(speed));
    Serial1.print("OK MOTOR ");
    Serial1.print(tankMotors[motorIndex].name);
    Serial1.print(' ');
    Serial1.println(clampPercent(speed));
    return;
  }

  String pinName = nextToken(line);
  int index = findPinIndex(pinName);
  if (index < 0) {
    sendError("BAD_PIN");
    return;
  }

  if (command == "SET") {
    uint32_t value = 0;
    if (!parseUint(nextToken(line), value) || value > 1) {
      sendError("BAD_VALUE");
      return;
    }
    setDigital(index, value);
    sendOkPinValue("SET", index, value);
    return;
  }

  if (command == "GET") {
    sendOkPinValue("GET", index, outputValue[index]);
    return;
  }

  if (command == "TOGGLE") {
    uint8_t value = outputValue[index] ? 0 : 1;
    setDigital(index, value);
    sendOkPinValue("TOGGLE", index, value);
    return;
  }

  if (command == "PWM") {
    uint32_t duty = 0;
    uint32_t hz = DEFAULT_PWM_HZ;
    String dutyToken = nextToken(line);
    String hzToken = nextToken(line);
    if (!parseUint(dutyToken, duty) || duty > PWM_MAX_DUTY) {
      sendError("BAD_DUTY");
      return;
    }
    if (hzToken.length() > 0 && !parseUint(hzToken, hz)) {
      sendError("BAD_FREQ");
      return;
    }
    if (setPwm(index, static_cast<uint16_t>(duty), hz)) {
      Serial1.print("OK PWM ");
      Serial1.print(pins[index].name);
      Serial1.print(' ');
      Serial1.print(duty);
      Serial1.print(' ');
      Serial1.println(hz);
    }
    return;
  }

  if (command == "ADCON") {
    uint32_t interval = ADC_DEFAULT_MS;
    String intervalToken = nextToken(line);
    if (intervalToken.length() > 0 && !parseUint(intervalToken, interval)) {
      sendError("BAD_INTERVAL");
      return;
    }
    configureAdc(index, static_cast<uint16_t>(interval));
    return;
  }

  if (command == "ADCOFF") {
    stopAdc(index);
    modeState[index] = MODE_IDLE;
    Serial1.print("OK ADCOFF ");
    Serial1.println(pins[index].name);
    return;
  }

  if (command == "ADCREAD") {
    if (!hasAdc(index)) {
      sendError("NO_ADC");
      return;
    }
    sendAdcSample(index);
    return;
  }

  sendError("BAD_COMMAND");
}

static void pollAdc() {
  uint32_t now = millis();
  for (size_t i = 0; i < PIN_COUNT; i++) {
    if (adcEnabled[i] && static_cast<int32_t>(now - adcNextDueMs[i]) >= 0) {
      sendAdcSample(i);
      adcNextDueMs[i] = now + adcIntervalMs[i];
    }
  }
}

static void pollTankDrive() {
  uint32_t now = millis();
  if (static_cast<int32_t>(now - driveNextStepMs) < 0) {
    return;
  }
  driveNextStepMs = now + DRIVE_STEP_MS;

  int8_t nextLeft = approachDriveValue(driveCurrentLeft, driveTargetLeft);
  int8_t nextRight = approachDriveValue(driveCurrentRight, driveTargetRight);
  if (nextLeft != driveCurrentLeft || nextRight != driveCurrentRight) {
    driveCurrentLeft = nextLeft;
    driveCurrentRight = nextRight;
    applyTankDrive(driveCurrentLeft, driveCurrentRight);
  }
}

void setup() {
  Serial1.begin(115200);
  inputLine.reserve(128);
  analogReadResolution(12);
  analogWriteResolution(12);
  analogWriteFrequency(DEFAULT_PWM_HZ);

  for (size_t i = 0; i < PIN_COUNT; i++) {
    modeState[i] = MODE_IDLE;
    outputValue[i] = 0;
    pwmFrequency[i] = DEFAULT_PWM_HZ;
    adcEnabled[i] = false;
    adcIntervalMs[i] = ADC_DEFAULT_MS;
    adcNextDueMs[i] = 0;
  }

  Serial1.println("READY STM32F103 UART GPIO PWM ADC");
  stopTankDrive();
}

void loop() {
  while (Serial1.available() > 0) {
    char c = static_cast<char>(Serial1.read());
    if (c == '\n' || c == '\r') {
      if (inputLine.length() > 0) {
        handleCommand(inputLine);
        inputLine = "";
      }
      continue;
    }

    if (inputLine.length() < 127) {
      inputLine += c;
    } else {
      inputLine = "";
      sendError("LINE_TOO_LONG");
    }
  }

  pollAdc();
  pollTankDrive();
}
