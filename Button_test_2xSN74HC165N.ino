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
  tft.init(240, 240, SPI_MODE2);  // Инициализация ST7789 дисплея 240x240
  tft.setRotation(4);             // Поворот экрана (при необходимости измените)
  tft.fillScreen(ST77XX_BLACK);   // Очистка экрана
  
  // Настройка шрифта и вывода приветствия
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(20, 100);
  tft.println("Testing buttons");
  delay(2000);
  tft.fillScreen(ST77XX_BLACK);
}

void loop() {
  // Считывание состояния кнопок
  uint16_t buttonStates = readShiftRegisters();
  
  // Вывод на последовательный монитор
  for (int i = 0; i < 14; i++) {
    Serial.print("Button ");
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.println(buttonStates & (1 << i) ? "PRESSED" : "RELEASED");
  }
  Serial.println("----------------");
  
  // Отображение на TFT-дисплее
  displayButtonStates(buttonStates);
  
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

// Функция для отображения состояния кнопок на TFT-дисплее
void displayButtonStates(uint16_t states) {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(0, 0);
  tft.println("Button States:");
  
  // Отображение состояния каждой кнопки
  for (int i = 0; i < 14; i++) {
    int x = (i % 7) * 34;
    int y = 30 + (i / 7) * 30;
    
    // Рисуем прямоугольник для кнопки
    if (states & (1 << i)) {
      tft.fillRect(x, y, 30, 25, ST77XX_GREEN);  // Нажата - зеленый
    } else {
      tft.fillRect(x, y, 30, 25, ST77XX_RED);    // Отпущена - красный
    }
    
    // Номер кнопки
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(x + 10, y + 8);
    tft.print(i + 1);
  }
  
  // Подпись
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(0, 220);
  tft.println("Green = Pressed  Red = Released");
}