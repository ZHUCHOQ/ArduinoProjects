#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Servo.h>

// Создаем объект для радио модуля
RF24 radio(9, 10); // CE, CSN

// Создаем объект для сервопривода
Servo myServo;

// Адрес для радио связи (должен совпадать с передатчиком)
const byte address[6] = "00001";

// Структура для приема данных (должна совпадать с передатчиком)
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

DataPacket rxData;

// Пин для подключения сервопривода
#define SERVO_PIN 3

// Переменные для диагностики
unsigned long lastDataTime = 0;
bool dataReceived = false;

void setup() {
  Serial.begin(9600);
  
  // Инициализация сервопривода
  myServo.attach(SERVO_PIN);
  myServo.write(90); // Установка в нейтральное положение
  
  // Инициализация радио модуля
  if (!radio.begin()) {
    Serial.println("Radio module not found! Check wiring.");
    while (1) {}
  }
  
  // Настройка радио модуля
  radio.openReadingPipe(0, address);
  radio.setPALevel(RF24_PA_MAX); // Уровень мощности передатчика
  radio.setDataRate(RF24_2MBPS); // Оптимальное значение скорости передачи данных
  radio.startListening(); // Начинаем прослушивание
  
  Serial.println("Receiver started. Waiting for data...");
  Serial.println("Use Joy1 X-axis to control the servo");
}

void loop() {
  // Проверяем, есть ли доступные данные
  if (radio.available()) {
    // Читаем данные
    radio.read(&rxData, sizeof(rxData));
    dataReceived = true;
    lastDataTime = millis();
    
    // Выводим полученные данные в монитор порта
    Serial.print("Joy1 X: ");
    Serial.print(rxData.joy1X);
    Serial.print(" | Servo position: ");
    
    // Управляем сервоприводом на основе положения джойстика 1 по оси X
    int servoPos = map(rxData.joy1X, 0, 1023, 0, 180);
    myServo.write(servoPos);
    
    Serial.println(servoPos);
  } else if (dataReceived && millis() - lastDataTime > 2000) {
    // Если данные не поступают более 2 секунд
    Serial.println("No data received for 2 seconds. Check transmitter.");
    dataReceived = false;
  }
  
  // Небольшая задержка для стабильности
  delay(50);
}