#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

// Определение пинов для дисплея
#define TFT_CS    3
#define TFT_DC    4
#define TFT_RST   5

// Определение пинов для регистров
const int loadPin = 8;   // SH/LD
const int clockPin = 2;  // CLK
const int dataPin = 6;   // QH первого регистра

// Инициализация дисплея
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// Переменные для хранения предыдущего состояния кнопок
uint16_t prevButtonStates = 0;
bool firstRun = true;

void setup() {
  // Инициализация последовательной связи
  Serial.begin(9600);
  
  // Инициализация пинов регистров
  pinMode(loadPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, INPUT);
  digitalWrite(clockPin, HIGH);
  digitalWrite(loadPin, HIGH);

  // Инициализация дисплея
  tft.init(240, 240, SPI_MODE2);
  tft.setRotation(4);
  tft.fillScreen(ST77XX_BLACK);
  
  // Рисуем статические элементы интерфейса
  drawStaticUI();
}

void loop() {
  // Считывание состояния кнопок
  uint16_t buttonStates = readShiftRegisters();
  
  // Вывод на последовательный монитор
  for (int i = 0; i < 16; i++) {
    Serial.print("Button ");
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.println(buttonStates & (1 << i) ? "PRESSED" : "RELEASED");
  }
  Serial.println("----------------");
  
  // Отображение на TFT-дисплее (только измененных состояний)
  updateButtonDisplay(buttonStates);
  
  delay(200); // Задержка между опросами
}

// Функция для чтения данных из сдвиговых регистров
uint16_t readShiftRegisters() {
  // Загрузка данных в регистры
  digitalWrite(loadPin, LOW);
  delayMicroseconds(5);
  digitalWrite(loadPin, HIGH);
  delayMicroseconds(5);

  // Считывание 16 бит
  uint16_t buttonStates = 0;
  for (int i = 0; i < 16; i++) {
    buttonStates = (buttonStates << 1) | digitalRead(dataPin);
    digitalWrite(clockPin, LOW);
    delayMicroseconds(5);
    digitalWrite(clockPin, HIGH);
    delayMicroseconds(5);
  }

  // Инверсия битов (так как кнопки подключены к GND)
  return ~buttonStates;
}

// Функция для рисования статических элементов интерфейса
void drawStaticUI() {
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(50, 5);
  tft.println("Button States");
  
  tft.drawLine(0, 30, 240, 30, ST77XX_WHITE); // Разделительная линия
  
  // Рисуем контуры всех кнопок (16 кнопок в сетке 4x4)
  for (int i = 0; i < 16; i++) {
    int x = 10 + (i % 4) * 55;  // 4 колонки
    int y = 40 + (i / 4) * 50;  // 4 строки
    
    tft.drawRect(x, y, 50, 40, ST77XX_WHITE); // Контур кнопки
    
    // Номер кнопки
    tft.setTextSize(1);
    tft.setCursor(x + 20, y + 15);
    tft.print(i + 1);
  }
  
  // Подпись внизу экрана
  tft.setTextSize(1);
  tft.setCursor(10, 220);
  tft.println("Green=Pressed  Red=Released");
}

// Функция для обновления отображения кнопок (только измененных состояний)
void updateButtonDisplay(uint16_t states) {
  // Если это первый запуск, рисуем все кнопки
  if (firstRun) {
    for (int i = 0; i < 16; i++) {
      drawButton(i, states & (1 << i));
    }
    firstRun = false;
    prevButtonStates = states;
    return;
  }
  
  // Проверяем, изменилось ли состояние каждой кнопки
  for (int i = 0; i < 16; i++) {
    bool currentState = states & (1 << i);
    bool prevState = prevButtonStates & (1 << i);
    
    // Если состояние изменилось, перерисовываем эту кнопку
    if (currentState != prevState) {
      drawButton(i, currentState);
    }
  }
  
  // Сохраняем текущее состояние для следующего сравнения
  prevButtonStates = states;
}

// Функция для отрисовки отдельной кнопки
void drawButton(int buttonIndex, bool isPressed) {
  int x = 10 + (buttonIndex % 4) * 55;  // 4 колонки
  int y = 40 + (buttonIndex / 4) * 50;  // 4 строки
  
  // Заливаем прямоугольник цветом в зависимости от состояния
  if (isPressed) {
    tft.fillRect(x + 1, y + 1, 48, 38, ST77XX_GREEN); // Нажата - зеленый
  } else {
    tft.fillRect(x + 1, y + 1, 48, 38, ST77XX_RED);   // Отпущена - красный
  }
  
  // Восстанавливаем номер кнопки
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(x + 20, y + 15);
  tft.print(buttonIndex + 1);
  
  // Восстанавливаем контур кнопки
  tft.drawRect(x, y, 50, 40, ST77XX_WHITE);
}