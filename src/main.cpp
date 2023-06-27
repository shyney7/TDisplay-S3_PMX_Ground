#define TOUCH_MODULES_CST_SELF
#include <Arduino.h>
#include <SPI.h>
#include <RF24.h>
#include <Wire.h>
#include "graph.h"
#include "TFT_eSPI.h"
#include "pin_config.h" //default TDisplay-S3 pin occupation
#include "TouchLib.h"
#include "fox.h"      // 78x81
#include "tagFont.h"  //Rock_Salt_Regular_10
#include <CircularBuffer.h>
#include "ringMeter.h"
#include <Button2.h>

//PMX Data Structure
struct nrfDataStruct{
  uint8_t change;
  float pm10;
  float sumBins;
  float temp;
  float altitude;
  float hum;
  float xtra;
}data;
//adc in use
char adcSelection[10];
//graph labels
String xLabel[5] = {"pm10","sum","temp","hum","adc0"}; //main window bar chart labels
int values[5] = {0}; // store pmx values to iterate over them
unsigned int x = 0; //graph iterator (track last 10 values)
CircularBuffer<int, 11> pm10Buffer;
uint8_t currentUIwindow = 0; //0 = main window, 1 = pm10 graph, 2 = sum graph, 3 = temp & hum graph, 4 = adc0 graph

//millis delay
unsigned long start_time, current_time, delay_time;

//functions prototypes
void getPMXdata();
void bootScreen();
void drawMainWindow();
void pm10Graph();
void tempHumGraph();
void btnHandler(Button2& btn);

//RF24
RF24 radio(3, 10); //CE, CSN
//const uint64_t pipe = 1; //PMX uses only the # 1 as pipe address, for whatever reason (imo it should be another random number to avoid collisions)

//TFT
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);

//Touch Wire1 in case other I2C devices are connected to Wire (most libs use Wire as default)
TouchLib touch(Wire1, PIN_IIC_SDA, PIN_IIC_SCL, CTS820_SLAVE_ADDRESS, PIN_TOUCH_RES);

//Buttons
Button2 btnUI;

void setup() {
  // init radio for reading
  Serial.begin(115200);
  if (!radio.begin()) {
    Serial.println("radio hardware is not responding!!!");
    //return;
  }
  radio.openReadingPipe(1, 1); //Tansmitter uses 0 as default pipe so we use 1 as receiver (0-5)
  radio.setChannel(108); //PMX channel
  radio.setDataRate(RF24_250KBPS); //PMX uses 250kbps (this is good for longer range)
  radio.setAutoAck(false);  //size is fixed so we don't need acknoledgement
  radio.setPALevel(RF24_PA_LOW);
  radio.startListening();

  // init buttons
  btnUI.begin(14);
  btnUI.setClickHandler(btnHandler);
  btnUI.setDoubleClickHandler(btnHandler);

  // init TFT and Touch
  Wire1.begin(PIN_IIC_SDA, PIN_IIC_SCL);
  pinMode(PIN_POWER_ON, OUTPUT);
  digitalWrite(PIN_POWER_ON, HIGH);
  pinMode(PIN_TOUCH_RES, OUTPUT);
  digitalWrite(PIN_TOUCH_RES, LOW);
  delay(500);
  digitalWrite(PIN_TOUCH_RES, HIGH);
  tft.begin();
  tft.fillScreen(TFT_BLACK);
  tft.setRotation(1);
  touch.setRotation(1);
  sprite.createSprite(320,170);
  sprite.setTextColor(TFT_WHITE,TFT_BLACK);
  sprite.fillSprite(TFT_BLACK);
  sprite.setSwapBytes(true);
  bootScreen();

  // init millis delay
  delay_time = 1000;
  current_time = millis();
  start_time = current_time;
  
}

void loop() {

  current_time = millis();
  btnUI.loop();
  if (current_time - start_time >= delay_time) {
    switch (currentUIwindow)
    {
    case 0:
      drawMainWindow();
      break;
    case 1:
      pm10Graph();
      break;
    case 2:
      tft.fillScreen(TFT_BLACK);
      tempHumGraph();
      break;
    case 3:
      drawMainWindow();
      break;
    case 4:
      pm10Graph();
      break;
    default:
      tft.fillScreen(TFT_BLACK);
      tft.drawString("error switching UI",0,0);
      break;
    }
    start_time = current_time;
  }
  //getPMXdata();
  //pm10Graph();
  //drawMainWindow();
  //tempHumGraph();

}

void getPMXdata() {
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
    if (data.change > 6 || data.change < 0) {
      strcpy(adcSelection, "error");
    }
    break;
  }
}

void drawMainWindow() {
  sprite.fillSprite(TFT_BLACK);
  //draw cooridnate system
  sprite.fillRect(5+16,34,2,119,TFT_WHITE); //y axis
  sprite.fillRect(5+16,153,40,2,TFT_WHITE); //x axis
  //draw other cooridnate system lines
  for (int i = 2; i <= 5; ++i) {
    sprite.fillRect(i*(5+16)+40*(i-1),34,2,119,TFT_WHITE); //y axis
    sprite.fillRect(i*(5+16)+40*(i-1),153,40,2,TFT_WHITE); //x axis
  }

  sprite.setTextDatum(4); //set text orientation to center instead of top left
  //draw y axis labels
  for (int i = 2; i < 12; i += 2) {
    sprite.drawString(String(i*10), 11, 153-(i*12), 1);
    for (int j = 0; j <284; j += 5) {
      sprite.drawPixel(21+j, 153-(i*12), TFT_WHITE);
    }
  }
  //draw x axis labels and bar chart
  for (int i = 0; i < 5; ++i) {
    sprite.drawString(xLabel[i], (i+1)* (5+16+20) +(i*20), 163, 2);
    values[i] = random(5, 100);
    int x = (i+1)* (5+16+20) +(i*20);
    sprite.fillRect(x-8, 153-(round(values[i]*1.2)), 16, round(values[i]*1.2), TFT_WHITE); //multiply by 1.2 to scale the values to the graph y axis is 119px long and values are 0-100
    sprite.drawString(String(values[i]), (i+1) * (5+16+20) +(i*20), 15, 2);
  }
  //push sprite to TFT
  sprite.pushSprite(0,0);
}

void pm10Graph() {

  if (x != 10) {
    pm10Buffer.push(random(0, 100));
    if (x == 0) {
      tft.fillScreen(TFT_BLACK);
      Graph(tft, x, pm10Buffer[x], 1, 40, 140, 260, 100, 0, 10, 1, 0, 100, 20, "", "", "", display1, YELLOW);
    }
    Trace(tft, x, pm10Buffer[x], 1, 40, 140, 260, 100, 0, 10, 1, 0, 100, 20, "PM10", "Last 10", "ug/m3", update1, YELLOW);
    ++x;
  }
  else {
    //plot buffer
    tft.fillScreen(TFT_BLACK);
    display1 = true;
    update1 = true;
    pm10Buffer.push(random(0, 100));
    Graph(tft, 0, pm10Buffer[0], 1, 40, 140, 260, 100, 0, 10, 1, 0, 100, 20, "", "", "", display1, YELLOW);
    for (decltype(pm10Buffer)::index_t i = 0; i < pm10Buffer.size(); ++i) {
      Trace(tft, i, pm10Buffer[i], 1, 40, 140, 260, 100, 0, 10, 1, 0, 100, 20, "PM10", "Last 10", "ug/m3", update1, YELLOW);
    }
  }
}

void bootScreen() {
  sprite.fillSprite(TFT_BLACK);
  sprite.setTextColor(TFT_WHITE, TFT_BLACK);
  sprite.setTextDatum(MC_DATUM);
  sprite.drawString("PMX Ground Station v1.0", 160, 20, 4);
  sprite.pushImage(121, 40, 78, 81, fox);
  sprite.drawString("by", 160, 130, 2);
  sprite.setFreeFont(&Rock_Salt_Regular_10);
  sprite.drawString("Marcel Oliveira", 160, 150);
  sprite.pushSprite(0,0);
  delay(3000);
  tft.fillScreen(TFT_BLACK);
  sprite.setTextDatum(0);
  sprite.setFreeFont();
}

void tempHumGraph() {
  //temp
  tft.setTextDatum(0);
  drawSaleSmallGauge(tft);
  hum_02 = random(0,100);
  tft.drawString("Humidity:", 10, 0, 4);
  tft.drawString(String(hum_02), 40, 30, 4);
  tft.drawString("%", 70, 30, 4);
  tft.drawString("100", 0, 55, 2);
  tft.drawString("40", 115, 155, 2);
  needleMeter(tft);
}

//button handler
void btnHandler(Button2 &btn) {
  switch (btn.getType()) {
    case single_click:
      if (currentUIwindow >= 4 || currentUIwindow < 0) {
        currentUIwindow = 0;
      }
      else {
        ++currentUIwindow;
      }
      break;
    case double_click:
      if (currentUIwindow <= 0) {
        currentUIwindow = 4;
      }
      else {
        --currentUIwindow;
      }
      break;
  }
}