"FIXME: MENU  мерцает, при этом основной экран нет"
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <EEPROM.h>
#include <RF24.h>

// Определение пинов для дисплея
#define TFT_CS    3
#define TFT_DC    4
#define TFT_RST   5

// Определение пинов для регистров
const int loadPin = 8;   // SH/LD
const int clockPin = 2;  // CLK
const int dataPin = 6;   // QH первого регистра

// Определение пинов для джойстиков
#define JOY1_X     A0
#define JOY1_Y     A1
#define JOY1_BTN   A2
#define JOY2_X     A3
#define JOY2_Y     A4
#define JOY2_BTN   A5

// Определение пинов для nRF24L01
#define CE_PIN     9
#define CSN_PIN    10

// Инициализация дисплея
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// Инициализация радио модуля
RF24 radio(CE_PIN, CSN_PIN);

// Адрес для радио связи
const byte address[6] = "00001";

// Структура для передачи данных
struct DataPacket {
  int joy1X;
  int joy1Y;
  int joy2X;
  int joy2Y;
  byte buttons;
  byte trim1X;
  byte trim1Y;
  byte trim2X;
  byte trim2Y;
};

DataPacket txData;

// Структура для хранения настроек в EEPROM
struct Settings {
  int joy1XCenter;
  int joy1YCenter;
  int joy2XCenter;
  int joy2YCenter;
  byte trim1X;
  byte trim1Y;
  byte trim2X;
  byte trim2Y;
  byte channelMapping;
};

Settings settings;

// Состояния меню
enum MenuState {
  MAIN_SCREEN,
  MAIN_MENU,
  CALIBRATION_MENU,
  TRIM_MENU,
  SETTINGS_MENU,
  RESET_CONFIRM
};

MenuState currentMenu = MAIN_SCREEN;

// Переменные для навигации по меню
int menuIndex = 0;
int subMenuIndex = 0;
const int menuItems = 5;
String mainMenu[menuItems] = {
  "Calibration",
  "Trim Settings",
  "Channel Map",
  "System Settings",
  "Exit"
};

// Переменные для кнопок
uint16_t buttonStates = 0;
uint16_t prevButtonStates = 0;

// Переменные для управления обновлением дисплея
unsigned long lastUpdateTime = 0;
const unsigned long updateInterval = 200;

// Переменные для хранения предыдущих значений
int prevJoy1X = 0, prevJoy1Y = 0, prevJoy2X = 0, prevJoy2Y = 0;
int prevTrim1X = 0, prevTrim1Y = 0, prevTrim2X = 0, prevTrim2Y = 0;

// Определение битовых масок для кнопок
#define BTN_UP     0
#define BTN_DOWN   1
#define BTN_LEFT   2
#define BTN_RIGHT  3
#define BTN_OK     4
#define BTN_BACK   5
#define BTN_MENU   6
#define BTN_EXTRA  7

// Прототипы функций
void readButtons();
void updateDisplay();
void handleMenuNavigation();
void processMainMenu();
void processCalibrationMenu();
void processTrimMenu();
void processSettingsMenu();
void showMainScreen();
void calibrateJoysticks();
void resetSettings();
void saveSettings();
void loadSettings();
void sendData();
void drawStaticElements();
void updateDynamicElements();
void drawMainMenu();
void drawCalibrationMenu();
void drawTrimMenu();
void drawSettingsMenu();
void drawResetConfirm();

void setup() {
  Serial.begin(9600);
  
  // Инициализация пинов регистров
  pinMode(loadPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, INPUT);
  digitalWrite(clockPin, HIGH);
  digitalWrite(loadPin, HIGH);
  
  // Настройка пинов кнопок джойстиков
  pinMode(JOY1_BTN, INPUT_PULLUP);
  pinMode(JOY2_BTN, INPUT_PULLUP);

  // Инициализация дисплея с поворотом на 180 градусов
  tft.init(240, 240, SPI_MODE2);
  tft.setRotation(2);  // Поворот на 180 градусов
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(50, 100);
  tft.println("RC Transmitter");
  delay(1000);

  // Инициализация радио модуля
  radio.begin();
  radio.openWritingPipe(address);
  radio.setPALevel(RF24_PA_MAX);
  radio.stopListening();

  // Загрузка настроек
  loadSettings();

  // Первоначальная калибровка
  if (settings.joy1XCenter == 0) {
    calibrateJoysticks();
  }

  // Рисуем статические элементы
  drawStaticElements();
}

void loop() {
  readButtons();
  handleMenuNavigation();
  
  // Чтение значений джойстиков
  txData.joy1X = analogRead(JOY1_X);
  txData.joy1Y = analogRead(JOY1_Y);
  txData.joy2X = analogRead(JOY2_X);
  txData.joy2Y = analogRead(JOY2_Y);
  
  // Применение триммов
  txData.joy1X = constrain(txData.joy1X + (settings.trim1X - 127) * 4, 0, 1023);
  txData.joy1Y = constrain(txData.joy1Y + (settings.trim1Y - 127) * 4, 0, 1023);
  txData.joy2X = constrain(txData.joy2X + (settings.trim2X - 127) * 4, 0, 1023);
  txData.joy2Y = constrain(txData.joy2Y + (settings.trim2Y - 127) * 4, 0, 1023);
  
  sendData();
  
  // Обновление дисплея по таймеру
  if (millis() - lastUpdateTime >= updateInterval) {
    updateDisplay();
    lastUpdateTime = millis();
  }
  
  delay(50);
}

void readButtons() {
  prevButtonStates = buttonStates;
  
  digitalWrite(loadPin, LOW);
  delayMicroseconds(5);
  digitalWrite(loadPin, HIGH);
  delayMicroseconds(5);

  buttonStates = 0;
  for (int i = 0; i < 16; i++) {
    buttonStates = (buttonStates << 1) | digitalRead(dataPin);
    digitalWrite(clockPin, LOW);
    delayMicroseconds(5);
    digitalWrite(clockPin, HIGH);
    delayMicroseconds(5);
  }
  
  buttonStates = ~buttonStates;
}

void updateDisplay() {
  switch (currentMenu) {
    case MAIN_SCREEN:
      updateDynamicElements(); // Только обновляем динамические элементы
      break;
    case MAIN_MENU:
      drawMainMenu();
      break;
    case CALIBRATION_MENU:
      drawCalibrationMenu();
      break;
    case TRIM_MENU:
      drawTrimMenu();
      break;
    case SETTINGS_MENU:
      drawSettingsMenu();
      break;
    case RESET_CONFIRM:
      drawResetConfirm();
      break;
  }
}

void drawStaticElements() {
  // Статические элементы главного экрана
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(60, 10);
  tft.println("RC Transmitter");
  
  // Рисуем статические подписи
  tft.setTextSize(1);
  tft.setCursor(10, 40);
  tft.print("Joy1 X: ");
  tft.setCursor(10, 60);
  tft.print("Joy1 Y: ");
  tft.setCursor(10, 80);
  tft.print("Joy2 X: ");
  tft.setCursor(10, 100);
  tft.print("Joy2 Y: ");
  tft.setCursor(10, 120);
  tft.print("Trim1 X: ");
  tft.setCursor(10, 140);
  tft.print("Trim1 Y: ");
  tft.setCursor(10, 160);
  tft.print("Trim2 X: ");
  tft.setCursor(10, 180);
  tft.print("Trim2 Y: ");
  
  tft.setCursor(10, 220);
  tft.println("Press MENU for options");
}

void updateDynamicElements() {
  // Обновляем только изменяющиеся значения
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  
  // Обновляем Joy1 X
  int joy1X = map(txData.joy1X, 0, 1023, -100, 100);
  if (joy1X != prevJoy1X) {
    tft.fillRect(70, 40, 40, 10, ST77XX_BLACK);
    tft.setCursor(70, 40);
    tft.print(joy1X);
    tft.print("%");
    prevJoy1X = joy1X;
  }
  
  // Обновляем Joy1 Y
  int joy1Y = map(txData.joy1Y, 0, 1023, -100, 100);
  if (joy1Y != prevJoy1Y) {
    tft.fillRect(70, 60, 40, 10, ST77XX_BLACK);
    tft.setCursor(70, 60);
    tft.print(joy1Y);
    tft.print("%");
    prevJoy1Y = joy1Y;
  }
  
  // Обновляем Joy2 X
  int joy2X = map(txData.joy2X, 0, 1023, -100, 100);
  if (joy2X != prevJoy2X) {
    tft.fillRect(70, 80, 40, 10, ST77XX_BLACK);
    tft.setCursor(70, 80);
    tft.print(joy2X);
    tft.print("%");
    prevJoy2X = joy2X;
  }
  
  // Обновляем Joy2 Y
  int joy2Y = map(txData.joy2Y, 0, 1023, -100, 100);
  if (joy2Y != prevJoy2Y) {
    tft.fillRect(70, 100, 40, 10, ST77XX_BLACK);
    tft.setCursor(70, 100);
    tft.print(joy2Y);
    tft.print("%");
    prevJoy2Y = joy2Y;
  }
  
  // Обновляем Trim1 X
  int trim1X = settings.trim1X - 127;
  if (trim1X != prevTrim1X) {
    tft.fillRect(70, 120, 40, 10, ST77XX_BLACK);
    tft.setCursor(70, 120);
    tft.print(trim1X);
    prevTrim1X = trim1X;
  }
  
  // Обновляем Trim1 Y
  int trim1Y = settings.trim1Y - 127;
  if (trim1Y != prevTrim1Y) {
    tft.fillRect(70, 140, 40, 10, ST77XX_BLACK);
    tft.setCursor(70, 140);
    tft.print(trim1Y);
    prevTrim1Y = trim1Y;
  }
  
  // Обновляем Trim2 X
  int trim2X = settings.trim2X - 127;
  if (trim2X != prevTrim2X) {
    tft.fillRect(70, 160, 40, 10, ST77XX_BLACK);
    tft.setCursor(70, 160);
    tft.print(trim2X);
    prevTrim2X = trim2X;
  }
  
  // Обновляем Trim2 Y
  int trim2Y = settings.trim2Y - 127;
  if (trim2Y != prevTrim2Y) {
    tft.fillRect(70, 180, 40, 10, ST77XX_BLACK);
    tft.setCursor(70, 180);
    tft.print(trim2Y);
    prevTrim2Y = trim2Y;
  }
}

void drawMainMenu() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(80, 10);
  tft.println("Menu");
  
  tft.setTextSize(1);
  for (int i = 0; i < menuItems; i++) {
    if (i == menuIndex) {
      tft.setTextColor(ST77XX_BLACK, ST77XX_WHITE);
    } else {
      tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    }
    tft.setCursor(20, 50 + i * 20);
    tft.println(mainMenu[i]);
  }
}

void drawCalibrationMenu() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(30, 80);
  tft.println("Calibration Mode");
  tft.setCursor(20, 120);
  tft.println("Set sticks to center");
  tft.setCursor(40, 160);
  tft.println("Press OK to save");
}

void drawTrimMenu() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(60, 100);
  tft.println("Trim Menu");
}

void drawSettingsMenu() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(50, 80);
  tft.println("Settings");
  tft.setCursor(30, 120);
  tft.println("Reset to defaults");
  tft.setCursor(30, 160);
  tft.println("Back to main menu");
}

void drawResetConfirm() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(30, 80);
  tft.println("Reset settings?");
  tft.setCursor(40, 120);
  tft.println("OK - Confirm");
  tft.setCursor(40, 160);
  tft.println("BACK - Cancel");
}

void handleMenuNavigation() {
  if (buttonStates & (1 << BTN_MENU) && !(prevButtonStates & (1 << BTN_MENU))) {
    if (currentMenu == MAIN_SCREEN) {
      currentMenu = MAIN_MENU;
      menuIndex = 0;
      drawMainMenu();
    } else {
      currentMenu = MAIN_SCREEN;
      drawStaticElements();
    }
  }
  
  switch (currentMenu) {
    case MAIN_MENU:
      processMainMenu();
      break;
    case CALIBRATION_MENU:
      processCalibrationMenu();
      break;
    case TRIM_MENU:
      processTrimMenu();
      break;
    case SETTINGS_MENU:
      processSettingsMenu();
      break;
    case RESET_CONFIRM:
      if (buttonStates & (1 << BTN_OK) && !(prevButtonStates & (1 << BTN_OK))) {
        resetSettings();
        currentMenu = MAIN_SCREEN;
        drawStaticElements();
      } else if (buttonStates & (1 << BTN_BACK) && !(prevButtonStates & (1 << BTN_BACK))) {
        currentMenu = SETTINGS_MENU;
        drawSettingsMenu();
      }
      break;
  }
}

void processMainMenu() {
  if (buttonStates & (1 << BTN_UP) && !(prevButtonStates & (1 << BTN_UP))) {
    menuIndex = (menuIndex - 1 + menuItems) % menuItems;
    drawMainMenu();
  }
  if (buttonStates & (1 << BTN_DOWN) && !(prevButtonStates & (1 << BTN_DOWN))) {
    menuIndex = (menuIndex + 1) % menuItems;
    drawMainMenu();
  }
  
  if (buttonStates & (1 << BTN_OK) && !(prevButtonStates & (1 << BTN_OK))) {
    switch (menuIndex) {
      case 0: 
        currentMenu = CALIBRATION_MENU;
        drawCalibrationMenu();
        break;
      case 1: 
        currentMenu = TRIM_MENU;
        drawTrimMenu();
        break;
      case 3: 
        currentMenu = SETTINGS_MENU;
        drawSettingsMenu();
        break;
      case 4: 
        currentMenu = MAIN_SCREEN;
        drawStaticElements();
        break;
    }
  }
}

void processCalibrationMenu() {
  if (buttonStates & (1 << BTN_OK) && !(prevButtonStates & (1 << BTN_OK))) {
    calibrateJoysticks();
    currentMenu = MAIN_MENU;
    drawMainMenu();
  }
  
  if (buttonStates & (1 << BTN_BACK) && !(prevButtonStates & (1 << BTN_BACK))) {
    currentMenu = MAIN_MENU;
    drawMainMenu();
  }
}

void processTrimMenu() {
  if (buttonStates & (1 << BTN_BACK) && !(prevButtonStates & (1 << BTN_BACK))) {
    currentMenu = MAIN_MENU;
    drawMainMenu();
  }
}

void processSettingsMenu() {
  if (buttonStates & (1 << BTN_UP) && !(prevButtonStates & (1 << BTN_UP))) {
    subMenuIndex = 0;
    drawSettingsMenu();
  }
  if (buttonStates & (1 << BTN_DOWN) && !(prevButtonStates & (1 << BTN_DOWN))) {
    subMenuIndex = 1;
    drawSettingsMenu();
  }
  
  if (buttonStates & (1 << BTN_OK) && !(prevButtonStates & (1 << BTN_OK))) {
    if (subMenuIndex == 0) {
      currentMenu = RESET_CONFIRM;
      drawResetConfirm();
    } else {
      currentMenu = MAIN_MENU;
      drawMainMenu();
    }
  }
}

void calibrateJoysticks() {
  settings.joy1XCenter = analogRead(JOY1_X);
  settings.joy1YCenter = analogRead(JOY1_Y);
  settings.joy2XCenter = analogRead(JOY2_X);
  settings.joy2YCenter = analogRead(JOY2_Y);
  
  settings.trim1X = 127;
  settings.trim1Y = 127;
  settings.trim2X = 127;
  settings.trim2Y = 127;
  
  saveSettings();
  
  // Сбрасываем предыдущие значения для обновления дисплея
  prevJoy1X = prevJoy1Y = prevJoy2X = prevJoy2Y = 0;
  prevTrim1X = prevTrim1Y = prevTrim2X = prevTrim2Y = 0;
  
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(40, 100);
  tft.println("Calibration");
  tft.setCursor(60, 140);
  tft.println("Complete!");
  delay(1000);
}

void resetSettings() {
  settings.joy1XCenter = 512;
  settings.joy1YCenter = 512;
  settings.joy2XCenter = 512;
  settings.joy2YCenter = 512;
  settings.trim1X = 127;
  settings.trim1Y = 127;
  settings.trim2X = 127;
  settings.trim2Y = 127;
  settings.channelMapping = 0x12;
  
  saveSettings();
  
  // Сбрасываем предыдущие значения для обновления дисплея
  prevJoy1X = prevJoy1Y = prevJoy2X = prevJoy2Y = 0;
  prevTrim1X = prevTrim1Y = prevTrim2X = prevTrim2Y = 0;
  
  calibrateJoysticks();
}

void saveSettings() {
  EEPROM.put(0, settings);
}

void loadSettings() {
  EEPROM.get(0, settings);
}

void sendData() {
  txData.buttons = buttonStates & 0xFF;
  txData.trim1X = settings.trim1X;
  txData.trim1Y = settings.trim1Y;
  txData.trim2X = settings.trim2X;
  txData.trim2Y = settings.trim2Y;
  
  radio.write(&txData, sizeof(txData));
}