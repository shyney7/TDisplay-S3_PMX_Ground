#define TOUCH_MODULES_CST_SELF
#define TOUCH_GET_FROM_INT 0
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
String xLabel[5] = {"pm10","sum","temp","hum","adc"}; //main window bar chart labels
float values[5] = {0}; // store pmx values to iterate over them
unsigned int xPM10 = 0; //graph iterator (track last 10 values)
unsigned int xSum = 0;
unsigned int xADC = 0;
CircularBuffer<float, 11> pm10Buffer;
CircularBuffer<float, 11> sumBuffer;
CircularBuffer<float, 11> adcBuffer;
uint8_t currentUIwindow = 0; //0 = main window, 1 = pm10 graph, 2 = sum graph, 3 = temp & hum graph, 4 = adc0 graph
int deb = 0; //debounce touch


//millis delay
unsigned long start_time, current_time, delay_time, start_timePMX, delay_timePMX, delay_timeTouch, start_timeTouch;

//functions prototypes
void getPMXdata();
void bootScreen();
void drawMainWindow();
void pm10Graph();
void sumBinsGraph ();
void adcGraph();
void tempHumGraph();
void btnHandler(Button2& btn);
void switchTouch();
void touchTask(void *pvParameters);

//RF24
RF24 radio(3, 10); //CE, CSN
//const uint64_t pipe = 1; //PMX uses only the # 1 as pipe address, for whatever reason (imo it should be another random number to avoid collisions)

//TFT
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);

//Touch Wire1 in case other I2C devices are connected to Wire (most libs use Wire as default)
TouchLib touch(Wire, PIN_IIC_SDA, PIN_IIC_SCL, CTS820_SLAVE_ADDRESS, PIN_TOUCH_RES);

//Buttons
Button2 btnUI;

//Thread 0 Task
TaskHandle_t Task1;

void setup() {
  // init radio for reading
  Serial.begin(115200);
  if (!radio.begin()) {
    Serial.println("radio hardware is not responding!!!");
    return;
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
  pinMode(PIN_POWER_ON, OUTPUT);
  digitalWrite(PIN_POWER_ON, HIGH);
  pinMode(PIN_TOUCH_RES, OUTPUT);
  digitalWrite(PIN_TOUCH_RES, LOW);
  delay(500);
  digitalWrite(PIN_TOUCH_RES, HIGH);
  tft.begin();
  tft.fillScreen(TFT_BLACK);
  tft.setRotation(1);
  sprite.createSprite(320,170);
  sprite.setTextColor(TFT_WHITE,TFT_BLACK);
  sprite.fillSprite(TFT_BLACK);
  sprite.setSwapBytes(true);
  Wire.begin(PIN_IIC_SDA, PIN_IIC_SCL);
  if (!touch.init()) {
    Serial.println("touch hardware is not responding!!!");
  }
  bootScreen();

  // touch task core 0
  xTaskCreatePinnedToCore(
    touchTask,   /* Task function. */
    "Task1",     /* name of task. */
    10000,       /* Stack size of task */
    NULL,        /* parameter of the task */
    1,           /* priority of the task */
    &Task1,      /* Task handle to keep track of created task */
    0);          /* pin task to core 0 */

  // init millis delay
  delay_time = 1000;
  delay_timePMX = 10;
  delay_timeTouch = 10;
  current_time = millis();
  start_time = current_time;
  start_timePMX = current_time;
  start_timeTouch = current_time;
  
}

void loop() {
  current_time = millis();

  if (current_time - start_timePMX >= delay_timePMX) {
    getPMXdata();
    start_timePMX = current_time;
  }

  btnUI.loop();
  if (current_time - start_timeTouch >= delay_timeTouch) {
    switchTouch();
    start_timeTouch = current_time;
  }

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
      sumBinsGraph();
      break;
    case 4:
      adcGraph();
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
    /* if (data.change > 6 || data.change < 0) {
      //strcpy(adcSelection, "error");
      //itoa(data.change, adcSelection, 10);
    } */
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
  values[0] = data.pm10;
  values[1] = data.sumBins;
  values[2] = data.temp;
  values[3] = data.hum;
  values[4] = data.xtra;
  //draw x axis labels and bar chart
  for (int i = 0; i < 5; ++i) {
    sprite.drawString(xLabel[i], (i+1)* (5+16+20) +(i*20), 163, 2);
    int x = (i+1)* (5+16+20) +(i*20);
    sprite.fillRect(x-8, 153-(round(values[i]*1.2)), 16, round(values[i]*1.2), TFT_WHITE); //multiply by 1.2 to scale the values to the y axis is 119px long and values are 0-100
    sprite.drawString(String(values[i]), (i+1) * (5+16+20) +(i*20), 15, 2);
  }
  //push sprite to TFT
  sprite.pushSprite(0,0);
}

void pm10Graph() {

  //fill buffer and draw graph
  if (xPM10 != 10) {
    pm10Buffer.push(data.pm10);
    if (xPM10 == 0) {
      display1 = true;
      update1 = true;
      tft.fillScreen(TFT_BLACK);
      Graph(tft, xPM10, pm10Buffer[xPM10], 1, 40, 140, 260, 100, 0, 10, 1, 0, 100, 20, "", "", "", display1, YELLOW);
    }
    Trace(tft, xPM10, pm10Buffer[xPM10], 1, 40, 140, 260, 100, 0, 10, 1, 0, 100, 20, "PM10", "Last 10", "ug/m3", update1, YELLOW);
    ++xPM10;
  }
  else {
    //plot fifo buffer
    tft.fillScreen(TFT_BLACK);
    display1 = true;
    update1 = true;
    pm10Buffer.push(data.pm10);
    Graph(tft, 0, pm10Buffer[0], 1, 40, 140, 260, 100, 0, 10, 1, 0, 100, 20, "", "", "", display1, YELLOW);
    for (decltype(pm10Buffer)::index_t i = 0; i < pm10Buffer.size(); ++i) {
      Trace(tft, i, pm10Buffer[i], 1, 40, 140, 260, 100, 0, 10, 1, 0, 100, 20, "PM10", "Last 10", "ug/m3", update1, YELLOW);
    }
  }
}

void sumBinsGraph() {
  //fill buffer and draw graph
  if (xSum != 10) {
    sumBuffer.push(data.sumBins);
    if (xSum == 0) {
      display1 = true;
      update1 = true;
      tft.fillScreen(TFT_BLACK);
      Graph(tft, xSum, sumBuffer[xSum], 1, 40, 140, 260, 100, 0, 10, 1, 0, 100, 20, "", "", "", display1, YELLOW);
    }
    Trace(tft, xSum, sumBuffer[xSum], 1, 40, 140, 260, 100, 0, 10, 1, 0, 100, 20, "Sum", "Last 10", "ug/m3", update1, YELLOW);
    ++xSum;
  }
  else {
    //plot fifo buffer
    tft.fillScreen(TFT_BLACK);
    display1 = true;
    update1 = true;
    sumBuffer.push(data.sumBins);
    Graph(tft, 0, sumBuffer[0], 1, 40, 140, 260, 100, 0, 10, 1, 0, 100, 20, "", "", "", display1, YELLOW);
    for (decltype(sumBuffer)::index_t i = 0; i < sumBuffer.size(); ++i) {
      Trace(tft, i, sumBuffer[i], 1, 40, 140, 260, 100, 0, 10, 1, 0, 100, 20, "Sum", "Last 10", "ug/m3", update1, YELLOW);
    }
  }
}

void adcGraph() {
  //fill buffer and draw graph
  if (xADC != 10) {
    adcBuffer.push(data.xtra);
    if (xADC == 0) {
      display1 = true;
      update1 = true;
      tft.fillScreen(TFT_BLACK);
      Graph(tft, xADC, adcBuffer[xADC], 1, 40, 140, 260, 100, 0, 10, 1, 0, 100, 20, "", "", "", display1, YELLOW);
    }
    Trace(tft, xADC, adcBuffer[xADC], 1, 40, 140, 260, 100, 0, 10, 1, 0, 100, 20, adcSelection, "Last 10", "ug/m3", update1, YELLOW);
    ++xADC;
  }
  else {
    //plot fifo buffer
    tft.fillScreen(TFT_BLACK);
    display1 = true;
    update1 = true;
    adcBuffer.push(data.xtra);
    Graph(tft, 0, adcBuffer[0], 1, 40, 140, 260, 100, 0, 10, 1, 0, 100, 20, "", "", "", display1, YELLOW);
    for (decltype(adcBuffer)::index_t i = 0; i < adcBuffer.size(); ++i) {
      Trace(tft, i, adcBuffer[i], 1, 40, 140, 260, 100, 0, 10, 1, 0, 100, 20, adcSelection, "Last 10", "ug/m3", update1, YELLOW);
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
  sprite.setFreeFont(&Rock_Salt_Regular_11);
  sprite.drawString("Marcel Oliveira", 160, 150);
  sprite.pushSprite(0,0);
  delay(3000);
  tft.fillScreen(TFT_BLACK);
  sprite.setTextDatum(0);
  sprite.setFreeFont();
}

void tempHumGraph() {
  //hum left needle gauge
  pivotNeedle_x = 30;
  center_x1 = 25;
  pivot_x = 30;
  tft.setTextDatum(0);
  drawScaleGaugeLeft(tft);
  tft.drawString("Humidity:", 10, 0, 4);
  tft.drawString(String(data.hum), 35, 30, 4);
  tft.drawString("%", 100, 30, 4);
  tft.drawString("100", 0, 55, 2);
  tft.drawString("40", 115, 155, 2);
  needleMeterLeft(tft, data.hum);
  //temp right needle gauge
  pivotNeedle_x = 290;
  center_x1 = 285;
  pivot_x = 290;
  drawScaleGaugeRight(tft);
  tft.setTextDatum(TR_DATUM);
  tft.drawString("Temperature:", 310, 0, 4);
  tft.drawString("`C", 285, 30, 4);
  tft.drawString(String(data.temp), 255, 30, 4);
  tft.drawString("-10", 205, 155, 2);
  tft.drawString("50", 320, 55, 2);
  tft.setTextDatum(0);
  needleMeterRight(tft, data.temp);
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
//do same as btnHandler but for touch
void switchTouch() {
    
    char str_buf[100];
    static uint8_t last_finger;
    if (touch.read()) {
      if (deb==0){
        deb=1;
        uint8_t n = touch.getPointNum();
        for (uint8_t i = 0; i < n; ++i) {
          TP_Point t = touch.getPoint(i);
          if (t.y >= 160 && t.y < 320) {
            if (currentUIwindow >= 4 || currentUIwindow < 0) {
              currentUIwindow = 0;
            }
            else {
              ++currentUIwindow;
            }
          }
          if (t.y < 160) {
            if (currentUIwindow <= 0) {
              currentUIwindow = 4;
            }
            else {
              --currentUIwindow;
            }
          }
        }
        last_finger = n;
      }
    }
    else {
      deb=0;
    }

}
//core 0 task for touch. Default loop is on core 1
void touchTask(void *pvParameters) {
  while (true) {
    switchTouch();
    delay(1);
  }
}