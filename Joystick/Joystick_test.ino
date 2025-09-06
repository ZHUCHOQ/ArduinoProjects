#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

// Определение пинов для дисплея
#define TFT_CS    3
#define TFT_DC    4
#define TFT_RST   5

// Определение пинов для джойстика
#define JOY_X     A0
#define JOY_Y     A1
#define JOY_BTN   A2

// Инициализация дисплея
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// Переменные для калибровки джойстика
int xMin = 1023, xMax = 0;
int yMin = 1023, yMax = 0;
int xCenter, yCenter;

// Переменные для хранения предыдущих значений
int prevX = 0, prevY = 0;
bool prevBtnState = true;
bool firstRun = true;
bool calibrating = false;

void setup() {
  // Инициализация последовательной связи
  Serial.begin(9600);
  
  // Настройка пина кнопки джойстика
  pinMode(JOY_BTN, INPUT_PULLUP);
  
  // Инициализация дисплея
  tft.init(240, 240, SPI_MODE2);
  tft.setRotation(4);
  tft.fillScreen(ST77XX_BLACK);
  
  // Первоначальная калибровка джойстика
  calibrateJoystick();
  
  // Рисуем статические элементы интерфейса
  drawStaticUI();
}

void loop() {
  // Чтение значений джойстика
  int xValue = analogRead(JOY_X);
  int yValue = analogRead(JOY_Y);
  bool buttonState = digitalRead(JOY_BTN);
  
  // Проверка нажатия кнопки для калибровки
  if (!buttonState && prevBtnState) {
    // Кнопка только что нажата
    calibrateJoystick();
    drawStaticUI(); // Перерисовываем статические элементы
    firstRun = true; // Сбрасываем флаг для полного обновления
  }
  
  // Вывод в Serial
  Serial.print("X: ");
  Serial.print(xValue);
  Serial.print(", Y: ");
  Serial.print(yValue);
  Serial.print(", Button: ");
  Serial.println(buttonState ? "OFF" : "ON");
  
  // Отображение на дисплее (только при изменении значений)
  updateDisplay(xValue, yValue, buttonState);
  
  // Сохраняем состояние кнопки для следующего цикла
  prevBtnState = buttonState;
  
  delay(100);
}

// Функция калибровки джойстика
void calibrateJoystick() {
  calibrating = true;
  
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(30, 100);
  tft.println("Calibrating...");
  tft.setCursor(10, 130);
  tft.println("Release joystick");
  tft.setCursor(30, 160);
  tft.println("and press button");
  
  // Ждем, пока кнопка будет отпущена
  while (!digitalRead(JOY_BTN)) {
    delay(10);
  }
  
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(30, 100);
  tft.println("Keep centered");
  tft.setCursor(50, 130);
  tft.println("and wait...");
  
  // Ждем 2 секунды для установки в нейтральное положение
  delay(2000);
  
  // Считываем текущие значения как центр
  xCenter = analogRead(JOY_X);
  yCenter = analogRead(JOY_Y);
  
  // Устанавливаем минимальные и максимальные значения относительно центра
  xMin = xCenter - 512;
  xMax = xCenter + 512;
  yMin = yCenter - 512;
  yMax = yCenter + 512;
  
  // Ограничиваем значения в пределах 0-1023
  xMin = constrain(xMin, 0, 1023);
  xMax = constrain(xMax, 0, 1023);
  yMin = constrain(yMin, 0, 1023);
  yMax = constrain(yMax, 0, 1023);
  
  // Вывод калибровочных значений в Serial
  Serial.print("X Center: "); Serial.println(xCenter);
  Serial.print("Y Center: "); Serial.println(yCenter);
  Serial.print("X Min: "); Serial.print(xMin);
  Serial.print(", X Max: "); Serial.println(xMax);
  Serial.print("Y Min: "); Serial.print(yMin);
  Serial.print(", Y Max: "); Serial.println(yMax);
  
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(50, 120);
  tft.println("Calibration complete!");
  delay(1000);
  
  calibrating = false;
}

// Функция отрисовки статических элементов интерфейса
void drawStaticUI() {
  tft.fillScreen(ST77XX_BLACK);
  
  // Отображение осей
  tft.drawLine(120, 30, 120, 210, ST77XX_WHITE);  // Вертикальная ось
  tft.drawLine(30, 120, 210, 120, ST77XX_WHITE);  // Горизонтальная ось
  
  // Центральная точка
  tft.fillCircle(120, 120, 3, ST77XX_WHITE);
  
  // Подписи осей
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(125, 25);
  tft.print("Y");
  tft.setCursor(125, 205);
  tft.print("Y");
  tft.setCursor(15, 115);
  tft.print("X");
  tft.setCursor(215, 115);
  tft.print("X");
  
  // Область для вывода значений
  tft.fillRect(30, 220, 180, 20, ST77XX_BLACK);
  
  // Область для состояния кнопки
  tft.fillRect(180, 20, 60, 20, ST77XX_BLACK);
  tft.setCursor(180, 20);
  tft.print("BTN:");
  
  // Инструкция по калибровке
  tft.setCursor(10, 240);
  tft.print("Press to calibrate");
}

// Функция обновления дисплея
void updateDisplay(int x, int y, bool btn) {
  if (calibrating) return;
  
  // Преобразуем значения для отображения на экране
  // Ось X: без инверсии
  int xScreen = map(x, xMin, xMax, 40, 200);
  // Ось Y: с инверсией
  int yScreen = map(y, yMin, yMax, 200, 40);
  
  // Ограничиваем значения в пределах экрана
  xScreen = constrain(xScreen, 40, 200);
  yScreen = constrain(yScreen, 40, 200);
  
  // Обновляем только если значения изменились
  if (x != prevX || y != prevY || firstRun) {
    // Стираем предыдущую точку
    if (!firstRun) {
      tft.fillCircle(prevX, prevY, 10, ST77XX_BLACK);
      // Восстанавливаем ось под точкой
      if (abs(prevX - 120) < 5 && abs(prevY - 120) < 5) {
        tft.fillCircle(120, 120, 3, ST77XX_WHITE);
      } else if (abs(prevX - 120) < 5) {
        tft.drawLine(118, 120, 122, 120, ST77XX_WHITE);
      } else if (abs(prevY - 120) < 5) {
        tft.drawLine(120, 118, 120, 122, ST77XX_WHITE);
      }
    }
    
    // Рисуем новую точку
    tft.fillCircle(xScreen, yScreen, 10, ST77XX_RED);
    
    // Сохраняем текущие значения
    prevX = xScreen;
    prevY = yScreen;
  }
  
  // Обновляем значения сопротивления
  tft.fillRect(30, 220, 180, 20, ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(30, 220);
  tft.print("X:");
  tft.print(x);
  tft.print(" Y:");
  tft.print(y);
  
  // Обновляем состояние кнопки
  if (btn != prevBtnState || firstRun) {
    tft.fillRect(215, 20, 25, 20, ST77XX_BLACK);
    tft.setCursor(215, 20);
    if (!btn) {
      tft.setTextColor(ST77XX_GREEN);
      tft.print("ON");
    } else {
      tft.setTextColor(ST77XX_RED);
      tft.print("OFF");
    }
  }
  
  if (firstRun) firstRun = false;
}