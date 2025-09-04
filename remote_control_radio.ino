#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <RF24.h>
#include <EEPROM.h>

// Пины дисплея
#define TFT_CS   3
#define TFT_DC   4
#define TFT_RST  5
#define TFT_BL   2

// Пины nRF24L01
#define CE_PIN   9
#define CSN_PIN  10

// Пины регистров сдвига
#define LATCH_PIN 8   // SH/LD
#define CLOCK_PIN 0   // CLK (RX)
#define DATA_PIN  1   // Q7 (TX)
#define ENABLE_PIN A6 // CLK INH

// Пины управления
#define MOTOR_PIN 7   // Вибромоторы
#define BUZZER_PIN 6  // Зуммер

// Аналоговые пины для джойстиков
#define JOY1_X A0
#define JOY1_Y A1
#define JOY2_X A2
#define JOY2_Y A3

// Создание объектов
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
RF24 radio(CE_PIN, CSN_PIN);

// Адрес для радио связи
const uint64_t address = 0xF0F0F0F0E1LL;

// Структура для передачи данных
struct ControlData {
  int16_t roll;     // Крен
  int16_t pitch;    // Тангаж
  int16_t throttle; // Газ
  int16_t yaw;      // Рыскание
  uint8_t buttons;  // Состояние кнопок
  uint8_t trim[4];  // Триммеры
};
ControlData controlData;

// Структура для калибровочных данных
struct CalibrationData {
  int16_t joy1X_center;
  int16_t joy1Y_center;
  int16_t joy2X_center;
  int16_t joy2Y_center;
  uint8_t checksum;
};
CalibrationData calibration;

// Состояния меню
enum MenuState {
  MAIN_SCREEN,
  SETTINGS,
  CALIBRATION,
  TRIM_SETTINGS,
  RESET_CONFIRM
};
MenuState menuState = MAIN_SCREEN;

// Переменные для навигации
int menuIndex = 0;
int trimIndex = 0;

// Состояние кнопок
uint16_t buttonState = 0;
uint16_t lastButtonState = 0;

// Определение битовых масок для кнопок
#define BTN_UP    0
#define BTN_DOWN  1
#define BTN_LEFT  2
#define BTN_RIGHT 3
#define BTN_OK    4
#define BTN_BACK  5
#define BTN_MENU  6
#define BTN_EXTRA 7

// Таймеры
unsigned long lastSendTime = 0;
unsigned long lastDisplayUpdate = 0;
const unsigned long SEND_INTERVAL = 20;
const unsigned long DISPLAY_INTERVAL = 500; // Увеличено для уменьшения мерцания

// Переменные для управления отрисовкой
bool needRedraw = true;
MenuState lastMenuState = MAIN_SCREEN;
int lastMenuIndex = 0;
int lastTrimIndex = 0;

void setup() {
  // Инициализация пинов
  pinMode(MOTOR_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH); // Включение подсветки
  
  // Инициализация регистров сдвига
  pinMode(LATCH_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(DATA_PIN, INPUT);
  pinMode(ENABLE_PIN, OUTPUT);
  digitalWrite(ENABLE_PIN, LOW);
  
  // Инициализация дисплея 240x240
  tft.init(240, 240);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setTextWrap(false);
  
  // Устанавливаем скорость SPI (может помочь с мерцанием)
  SPI.setClockDivider(SPI_CLOCK_DIV2);
  
  // Инициализация радио
  radio.begin();
  radio.setDataRate(RF24_250KBPS);
  radio.openWritingPipe(address);
  radio.setPALevel(RF24_PA_MAX);
  radio.stopListening();
  
  // Загрузка калибровочных данных
  loadCalibration();
  
  // Проверка контрольной суммы
  if (calibration.checksum != calculateChecksum()) {
    // Значения по умолчанию
    calibration.joy1X_center = 512;
    calibration.joy1Y_center = 512;
    calibration.joy2X_center = 512;
    calibration.joy2Y_center = 512;
    calibration.checksum = calculateChecksum();
    saveCalibration();
  }
  
  // Инициализация триммеров
  for (int i = 0; i < 4; i++) {
    controlData.trim[i] = 0;
  }
  
  // Приветственное сообщение
  showSplashScreen();
}

void loop() {
  // Опрос кнопок
  readButtons();
  
  // Обработка навигации
  handleMenuNavigation();
  
  // Чтение джойстиков
  readJoysticks();
  
  // Отправка данных
  unsigned long currentTime = millis();
  if (currentTime - lastSendTime >= SEND_INTERVAL) {
    sendData();
    lastSendTime = currentTime;
  }
  
  // Обновление дисплея (реже для уменьшения мерцания)
  if (currentTime - lastDisplayUpdate >= DISPLAY_INTERVAL) {
    // Проверяем, нужно ли перерисовывать экран
    if (needRedraw || menuState != lastMenuState || 
        menuIndex != lastMenuIndex || trimIndex != lastTrimIndex) {
      updateDisplay();
      lastDisplayUpdate = currentTime;
      needRedraw = false;
      lastMenuState = menuState;
      lastMenuIndex = menuIndex;
      lastTrimIndex = trimIndex;
    }
  }
}

void showSplashScreen() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(3);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(40, 100);
  tft.print("RC Transmitter");
  
  // Короткий звуковой сигнал
  digitalWrite(BUZZER_PIN, HIGH);
  delay(100);
  digitalWrite(BUZZER_PIN, LOW);
  
  // Вибрация
  digitalWrite(MOTOR_PIN, HIGH);
  delay(200);
  digitalWrite(MOTOR_PIN, LOW);
  
  delay(1500);
  tft.fillScreen(ST77XX_BLACK);
  needRedraw = true;
}

void readButtons() {
  lastButtonState = buttonState;
  
  digitalWrite(LATCH_PIN, LOW);
  delayMicroseconds(5);
  digitalWrite(LATCH_PIN, HIGH);
  
  buttonState = 0;
  for (int i = 0; i < 16; i++) {
    buttonState |= (digitalRead(DATA_PIN) << i);
    digitalWrite(CLOCK_PIN, HIGH);
    delayMicroseconds(5);
    digitalWrite(CLOCK_PIN, LOW);
  }
}

void handleMenuNavigation() {
  bool upPressed = (buttonState & (1 << BTN_UP)) && !(lastButtonState & (1 << BTN_UP));
  bool downPressed = (buttonState & (1 << BTN_DOWN)) && !(lastButtonState & (1 << BTN_DOWN));
  bool leftPressed = (buttonState & (1 << BTN_LEFT)) && !(lastButtonState & (1 << BTN_LEFT));
  bool rightPressed = (buttonState & (1 << BTN_RIGHT)) && !(lastButtonState & (1 << BTN_RIGHT));
  bool okPressed = (buttonState & (1 << BTN_OK)) && !(lastButtonState & (1 << BTN_OK));
  bool backPressed = (buttonState & (1 << BTN_BACK)) && !(lastButtonState & (1 << BTN_BACK));
  bool menuPressed = (buttonState & (1 << BTN_MENU)) && !(lastButtonState & (1 << BTN_MENU));
  
  switch (menuState) {
    case MAIN_SCREEN:
      if (menuPressed) {
        menuState = SETTINGS;
        menuIndex = 0;
        needRedraw = true;
        beep();
      }
      break;
      
    case SETTINGS:
      if (upPressed) {
        menuIndex = (menuIndex - 1 + 4) % 4;
        needRedraw = true;
        beep();
      } else if (downPressed) {
        menuIndex = (menuIndex + 1) % 4;
        needRedraw = true;
        beep();
      } else if (okPressed) {
        beep();
        if (menuIndex == 0) {
          menuState = CALIBRATION;
          needRedraw = true;
        } else if (menuIndex == 1) {
          menuState = TRIM_SETTINGS;
          trimIndex = 0;
          needRedraw = true;
        } else if (menuIndex == 2) {
          menuState = RESET_CONFIRM;
          needRedraw = true;
        } else if (menuIndex == 3) {
          menuState = MAIN_SCREEN;
          needRedraw = true;
        }
      } else if (backPressed) {
        menuState = MAIN_SCREEN;
        needRedraw = true;
        beep();
      }
      break;
      
    case CALIBRATION:
      if (okPressed) {
        calibrateJoysticks();
        saveCalibration();
        menuState = SETTINGS;
        needRedraw = true;
        beep();
      } else if (backPressed) {
        menuState = SETTINGS;
        needRedraw = true;
        beep();
      }
      break;
      
    case TRIM_SETTINGS:
      if (upPressed) {
        trimIndex = (trimIndex - 1 + 5) % 5;
        needRedraw = true;
        beep();
      } else if (downPressed) {
        trimIndex = (trimIndex + 1) % 5;
        needRedraw = true;
        beep();
      } else if (leftPressed && trimIndex < 4) {
        controlData.trim[trimIndex] -= 5;
        needRedraw = true;
        beep();
      } else if (rightPressed && trimIndex < 4) {
        controlData.trim[trimIndex] += 5;
        needRedraw = true;
        beep();
      } else if (okPressed) {
        if (trimIndex == 4) {
          menuState = SETTINGS;
          needRedraw = true;
        }
        beep();
      } else if (backPressed) {
        menuState = SETTINGS;
        needRedraw = true;
        beep();
      }
      break;
      
    case RESET_CONFIRM:
      if (okPressed) {
        resetToDefaults();
        menuState = SETTINGS;
        needRedraw = true;
        beep();
      } else if (backPressed) {
        menuState = SETTINGS;
        needRedraw = true;
        beep();
      }
      break;
  }
}

void readJoysticks() {
  int joy1X = analogRead(JOY1_X);
  int joy1Y = analogRead(JOY1_Y);
  int joy2X = analogRead(JOY2_X);
  int joy2Y = analogRead(JOY2_Y);
  
  controlData.roll = map(joy1X, 0, 1023, -512, 512) - (calibration.joy1X_center - 512);
  controlData.pitch = map(joy1Y, 0, 1023, -512, 512) - (calibration.joy1Y_center - 512);
  controlData.throttle = map(joy2Y, 0, 1023, 0, 1023);
  controlData.yaw = map(joy2X, 0, 1023, -512, 512) - (calibration.joy2X_center - 512);
  
  controlData.roll += controlData.trim[0];
  controlData.pitch += controlData.trim[1];
  controlData.throttle += controlData.trim[2];
  controlData.yaw += controlData.trim[3];
  
  controlData.roll = constrain(controlData.roll, -512, 512);
  controlData.pitch = constrain(controlData.pitch, -512, 512);
  controlData.throttle = constrain(controlData.throttle, 0, 1023);
  controlData.yaw = constrain(controlData.yaw, -512, 512);
}

void sendData() {
  controlData.buttons = buttonState & 0xFF;
  radio.write(&controlData, sizeof(controlData));
}

void updateDisplay() {
  tft.fillScreen(ST77XX_BLACK);
  
  switch (menuState) {
    case MAIN_SCREEN:
      drawMainScreen();
      break;
      
    case SETTINGS:
      drawSettingsScreen();
      break;
      
    case CALIBRATION:
      drawCalibrationScreen();
      break;
      
    case TRIM_SETTINGS:
      drawTrimSettingsScreen();
      break;
      
    case RESET_CONFIRM:
      drawResetConfirmScreen();
      break;
  }
}

void drawMainScreen() {
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  
  // Отображение значений джойстиков
  tft.setCursor(10, 10);
  tft.print("Roll: ");
  tft.print(controlData.roll);
  tft.print("  ");
  
  tft.setCursor(10, 40);
  tft.print("Pitch: ");
  tft.print(controlData.pitch);
  tft.print("  ");
  
  tft.setCursor(10, 70);
  tft.print("Thr: ");
  tft.print(controlData.throttle);
  tft.print("  ");
  
  tft.setCursor(10, 100);
  tft.print("Yaw: ");
  tft.print(controlData.yaw);
  tft.print("  ");
  
  // Отображение триммеров
  tft.setTextSize(1);
  tft.setCursor(10, 130);
  tft.print("Trim R:");
  tft.print(controlData.trim[0]);
  tft.print(" P:");
  tft.print(controlData.trim[1]);
  
  tft.setCursor(10, 150);
  tft.print("Trim T:");
  tft.print(controlData.trim[2]);
  tft.print(" Y:");
  tft.print(controlData.trim[3]);
  
  // Инструкция
  tft.setTextSize(1);
  tft.setCursor(60, 200);
  tft.print("Menu ->");
}

void drawSettingsScreen() {
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(60, 10);
  tft.print("SETTINGS");
  
  for (int i = 0; i < 4; i++) {
    tft.setCursor(40, 50 + i * 30);
    if (i == menuIndex) {
      tft.setTextColor(ST77XX_RED);
      tft.print("> ");
    } else {
      tft.setTextColor(ST77XX_WHITE);
      tft.print("  ");
    }
    
    switch(i) {
      case 0: tft.print("Calibration"); break;
      case 1: tft.print("Trimmers"); break;
      case 2: tft.print("Reset Settings"); break;
      case 3: tft.print("Back"); break;
    }
  }
}

void drawCalibrationScreen() {
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(60, 10);
  tft.print("CALIBRATION");
  
  tft.setTextSize(1);
  tft.setCursor(20, 80);
  tft.print("Set joysticks to center");
  tft.setCursor(20, 100);
  tft.print("position and press OK");
}

void drawTrimSettingsScreen() {
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(70, 10);
  tft.print("TRIM");
  
  for (int i = 0; i < 5; i++) {
    tft.setCursor(40, 50 + i * 30);
    if (i == trimIndex) {
      tft.setTextColor(ST77XX_RED);
      tft.print("> ");
    } else {
      tft.setTextColor(ST77XX_WHITE);
      tft.print("  ");
    }
    
    if (i < 4) {
      switch(i) {
        case 0: tft.print("Roll: "); break;
        case 1: tft.print("Pitch: "); break;
        case 2: tft.print("Throttle: "); break;
        case 3: tft.print("Yaw: "); break;
      }
      tft.print(controlData.trim[i]);
    } else {
      tft.print("Back");
    }
  }
}

void drawResetConfirmScreen() {
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(30, 10);
  tft.print("CONFIRM RESET");
  
  tft.setTextSize(1);
  tft.setCursor(40, 80);
  tft.print("Reset all settings?");
  tft.setCursor(40, 110);
  tft.print("OK - Yes, BACK - No");
}

void calibrateJoysticks() {
  calibration.joy1X_center = analogRead(JOY1_X);
  calibration.joy1Y_center = analogRead(JOY1_Y);
  calibration.joy2X_center = analogRead(JOY2_X);
  calibration.joy2Y_center = analogRead(JOY2_Y);
  calibration.checksum = calculateChecksum();
}

void saveCalibration() {
  EEPROM.put(0, calibration);
}

void loadCalibration() {
  EEPROM.get(0, calibration);
}

uint8_t calculateChecksum() {
  uint8_t sum = 0;
  uint8_t* data = (uint8_t*)&calibration;
  for (size_t i = 0; i < sizeof(calibration) - 1; i++) {
    sum += data[i];
  }
  return sum;
}

void resetToDefaults() {
  calibration.joy1X_center = 512;
  calibration.joy1Y_center = 512;
  calibration.joy2X_center = 512;
  calibration.joy2Y_center = 512;
  calibration.checksum = calculateChecksum();
  saveCalibration();
  
  for (int i = 0; i < 4; i++) {
    controlData.trim[i] = 0;
  }
}

void beep() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(50);
  digitalWrite(BUZZER_PIN, LOW);
}