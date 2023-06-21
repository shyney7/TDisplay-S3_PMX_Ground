#define TOUCH_MODULES_CST_SELF
#include <Arduino.h>
#include <SPI.h>
#include <RF24.h>
#include <Wire.h>
#include "TFT_eSPI.h"
#include "pin_config.h"
#include "TouchLib.h"
#include "fox.h"      // 78x81
#include "tagFont.h"  //Rock_Salt_Regular_10

struct nrfDataStruct{
  uint8_t change;
  float pm10;
  float sumBins;
  float temp;
  float altitude;
  float hum;
  float xtra;
}data;
char adcSelection[10];

//RF24
RF24 radio(3, 10); //CE, CSN
//const uint64_t pipe = 1;

//TFT
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);

//Touch
TouchLib touch(Wire1, PIN_IIC_SDA, PIN_IIC_SCL, CTS820_SLAVE_ADDRESS, PIN_TOUCH_RES);

void setup() {
  // init radio for reading
  Serial.begin(115200);
  if (!radio.begin()) {
    Serial.println("radio hardware is not responding!!!");
    return;
  }
  radio.openReadingPipe(1, 1);
  radio.setChannel(108);
  radio.setDataRate(RF24_250KBPS);
  radio.setAutoAck(false);
  radio.setPALevel(RF24_PA_LOW);
  radio.startListening();

  Wire1.begin(PIN_IIC_SDA, PIN_IIC_SCL);
  pinMode(PIN_POWER_ON, OUTPUT);
  digitalWrite(PIN_POWER_ON, HIGH);
  pinMode(PIN_TOUCH_RES, OUTPUT);
  digitalWrite(PIN_TOUCH_RES, LOW);
  delay(500);
  digitalWrite(PIN_TOUCH_RES, HIGH);
  tft.begin();
  tft.setRotation(1);
  touch.setRotation(1);
  sprite.createSprite(320,170);
  sprite.setTextColor(TFT_WHITE,TFT_BLACK);
  sprite.fillSprite(TFT_BLACK);
  sprite.setSwapBytes(true);

}

void loop() {

  if (radio.available()) {
    radio.read(&data, sizeof(data));
  }

  switch (data.change)
  {
  case 0:
    strcpy(adcSelection, "N/A");
    break;
  case 1:
    strcpy(adcSelection, "ADC0");
    break;
  case 2:
    strcpy(adcSelection, "ADC1");
    break;
  case 3:
    strcpy(adcSelection, "ADC2");
    break;
  case 4:
    strcpy(adcSelection, "ADC3");
    break;
  case 5:
    strcpy(adcSelection, "ADC01Diff");
    break;
  case 6:
    strcpy(adcSelection, "ADC23Diff");
    break;
  default:
    if (data.change > 6) {
      strcpy(adcSelection, "error");
    }
    break;
  }

  sprite.fillSprite(TFT_BLACK);
  sprite.drawString("PM10", 35, 10, 2);
  sprite.drawString("Sum", 145, 10, 2);
  sprite.drawString("Temp", 255, 10, 2);
  sprite.drawString(String(data.pm10), 0, 30, 7);
  sprite.drawString(String(data.sumBins), 110, 30, 7);
  sprite.drawString(String(data.temp), 220, 30, 7);
  sprite.drawString("Alt", 35, 85, 2);
  sprite.drawString("Hum", 145, 85, 2);
  sprite.drawString(adcSelection, 255, 85, 2);
  sprite.drawString(String(data.altitude), 0, 105, 7);
  sprite.drawString(String(data.hum), 110, 105, 7);
  sprite.drawString(String(data.xtra), 220, 105, 7);
  //sprite.pushImage(242, 65, 78, 81, fox);
  sprite.setFreeFont(&Rock_Salt_Regular_10);
  sprite.drawString("MAR", 270, 155);
  sprite.pushSprite(0,0);

}
