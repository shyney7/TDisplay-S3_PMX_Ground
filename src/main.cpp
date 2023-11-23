#define TOUCH_MODULES_CST_SELF
#define TOUCH_GET_FROM_INT 0
#include <Arduino.h>
#include <SPI.h>
#include <RF24Network.h>
#include <RF24.h>
#include <RF24Mesh.h>
#include <Wire.h>
#include "graph.h"
#include "TFT_eSPI.h"
#include "pin_config.h" //default TDisplay-S3 pin occupation
#include "TouchLib.h"
#include "fox.h"      // 78x81
#include "tagFont.h"  //Rock_Salt_Regular_10
#include "wifiGif.h"
#include <CircularBuffer.h>
#include "ringMeter.h"
#include <Button2.h>
#include <pthread.h>

//PMX Data Structure
struct nrfDataStruct{
  uint8_t change;
  float pm10;
  float sumBins;
  float temp;
  float altitude;
  float hum;
  float xtra;
  float co2;
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  double lat;
  double lng;
}data;
//adc in use
char adcSelection[10];
//graph labels
String xLabel[5] = {"pm10","sum","temp","hum","adc"}; //main window bar chart labels
float values[5] = {0}; // store pmx values to iterate over them
CircularBuffer<float, 11> pm10Buffer;
CircularBuffer<float, 11> sumBuffer;
CircularBuffer<float, 11> adcBuffer;
volatile uint8_t currentUIwindow = 0; //0 = main window, 1 = pm10 graph, 2 = sum graph, 3 = temp & hum graph, 4 = adc0 graph
int deb = 0; //debounce touch
double yhigh, yinc; //graph y axis
//uint32_t volt; //battery voltage


//millis delay
unsigned long start_time, current_time, delay_time, start_timePMX, delay_timePMX, delay_timeTouch, start_timeTouch;

//functions prototypes
void getPMXdata();
void bootScreen();
void clearScreen();
void noRFscreen();
void drawMainWindow();
void pm10Graph();
void sumBinsGraph ();
void adcGraph();
void tempHumGraph();
void btnHandler(Button2& btn);
void switchTouch();
void touchTask(void *pvParameters);
void fillBuffers();
bool checkBuffer100(CircularBuffer<float, 11> &buffer);
float maxBuffer(CircularBuffer<float, 11> &buffer);

//RF24
RF24 radio(3, 10); //CE, CSN
RF24Network network(radio);
RF24Mesh mesh(radio, network);
//const uint64_t pipe = 1; //PMX uses only the # 1 as pipe address, for whatever reason (imo it should be another random number to avoid collisions)

//TFT
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);

//Touch Wire1 in case other I2C devices are connected to Wire (most libs use Wire as default)
TouchLib touch(Wire, PIN_IIC_SDA, PIN_IIC_SCL, CTS820_SLAVE_ADDRESS, PIN_TOUCH_RES);

//Buttons
Button2 btnUI;

//Thread 0 Task
static pthread_mutex_t uiMutex;
TaskHandle_t Task1;

void setup() {
  // init radio for reading
  Serial.begin(115200);

  //init mutex
  if (pthread_mutex_init(&uiMutex, NULL) != 0) {
    Serial.println("uiMutex init failed");
  }

  // init buttons
  btnUI.begin(14);
  btnUI.setClickHandler(btnHandler);
  btnUI.setDoubleClickHandler(btnHandler);

  // init TFT and Touch
  pinMode(PIN_TOUCH_INT, INPUT_PULLUP);
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

  // fill buffers
  fillBuffers();

  //clear screen
  delay(3000);
  clearScreen();
  // set nodeID to 0 for the master node
  mesh.setNodeID(0);
  //check RF24 connection
  while(!radio.begin()) {
    noRFscreen();
  }
  clearScreen();
  //radio.openReadingPipe(1, 1); //Tansmitter uses 0 as default pipe so we use 1 as receiver (0-5)
  //radio.setChannel(108); //PMX channel
  //radio.setDataRate(RF24_250KBPS); //PMX uses 250kbps (this is good for longer range)
  //radio.setAutoAck(false);  //size is fixed so we don't need acknoledgement
  radio.setPALevel(RF24_PA_MAX);
  //radio.startListening();
  while (!mesh.begin(MESH_DEFAULT_CHANNEL, RF24_250KBPS))
  {
    noRFscreen();
  }
  clearScreen();
  
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

  mesh.update();
  mesh.DHCP();

  current_time = millis();

   if (current_time - start_timePMX >= delay_timePMX) {
    getPMXdata();
    start_timePMX = current_time;
    Serial.println("CO2: " + String(data.co2) + "\nDate: ");
    Serial.println(String(data.day) + "/" + String(data.month) + "/" + String(data.year) + " " + String(data.hour) + ":" + String(data.minute) + ":" + String(data.second));
    Serial.print("Lat: "); Serial.println(data.lat, 6);
    Serial.print("Lng: "); Serial.println(data.lng, 6);

  }

  btnUI.loop();
/*   if (current_time - start_timeTouch >= delay_timeTouch) {
    switchTouch();
    start_timeTouch = current_time;
  } */
  //DEBUG BLOCK
/*   strcpy(adcSelection, "ADC2");
  data.change = 2;
  data.altitude = random(0,100);
  data.hum = random(40,60);
  data.pm10 = random(0,100);
  data.sumBins = random(0,100);
  data.temp = random(20,30);
  data.xtra = random(0,100); */

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
//get data from PMX and check which adc is in use
void getPMXdata() {
/*   if (radio.available()) {
    radio.read(&data, sizeof(data));
  } */
  if (network.available()) {
    RF24NetworkHeader header;
    network.peek(header);

    switch (header.type) {
      case 'A':
        network.read(header, &data, sizeof(data));
        break;
      default:
        network.read(header, 0, 0);
        Serial.println(header.type);
        break;
    }
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
//draw bar chart window
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

  //plot fifo buffer
  tft.fillScreen(TFT_BLACK);
  display1 = true;
  update1 = true;
  pm10Buffer.push(data.pm10);
  if(checkBuffer100(pm10Buffer)) {
    yhigh = (double) maxBuffer(pm10Buffer);
    yinc = yhigh/5;
  }
  else {
    yhigh = 100;
    yinc = 20;
  }
  //volt = (analogRead(4) * 2 * 3.3 * 1000) / 4096;
  //tft.drawString(String(volt)+" mV", 300, 5);
  Graph(tft, 0, pm10Buffer[0], 1, 40, 140, 260, 100, 0, 10, 1, 0, yhigh, yinc, "", "", "", display1, YELLOW);
  for (decltype(pm10Buffer)::index_t i = 0; i < pm10Buffer.size(); ++i) {
    Trace(tft, i, pm10Buffer[i], 1, 40, 140, 260, 100, 0, 10, 1, 0, yhigh, yinc, "PM10", "Last 10", "ug/m3", update1, YELLOW);
  }

}

void sumBinsGraph() {
  //plot fifo buffer
  tft.fillScreen(TFT_BLACK);
  display1 = true;
  update1 = true;
  sumBuffer.push(data.sumBins);
  if(checkBuffer100(sumBuffer)) {
    yhigh = (double) maxBuffer(sumBuffer);
    yinc = yhigh/5;
  }
  else {
    yhigh = 100;
    yinc = 20;
  }
  Graph(tft, 0, sumBuffer[0], 1, 40, 140, 260, 100, 0, 10, 1, 0, yhigh, yinc, "", "", "", display1, YELLOW);
  for (decltype(sumBuffer)::index_t i = 0; i < sumBuffer.size(); ++i) {
    Trace(tft, i, sumBuffer[i], 1, 40, 140, 260, 100, 0, 10, 1, 0, yhigh, yinc, "Sum", "Last 10", "ug/m3", update1, YELLOW);
  }
}

void adcGraph() {
  //plot fifo buffer
  tft.fillScreen(TFT_BLACK);
  display1 = true;
  update1 = true;
  adcBuffer.push(data.xtra);
  if(checkBuffer100(adcBuffer)) {
    yhigh = (double) maxBuffer(adcBuffer);
    yinc = yhigh/5;
  }
  else {
    yhigh = 100;
    yinc = 20;
  }
  Graph(tft, 0, adcBuffer[0], 1, 40, 140, 260, 100, 0, 10, 1, 0, yhigh, yinc, "", "", "", display1, YELLOW);
  for (decltype(adcBuffer)::index_t i = 0; i < adcBuffer.size(); ++i) {
    Trace(tft, i, adcBuffer[i], 1, 40, 140, 260, 100, 0, 10, 1, 0, yhigh, yinc, adcSelection, "Last 10", "Raw", update1, YELLOW);
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
  //delay(3000);
  //tft.fillScreen(TFT_BLACK);
  //sprite.setTextDatum(0);
  //sprite.setFreeFont();
}
//clear screen & reset text orientation + font
void clearScreen() {
  tft.fillScreen(TFT_BLACK);
  sprite.fillSprite(TFT_BLACK);
  sprite.setTextDatum(0);
  sprite.setFreeFont();
  tft.setTextDatum(0);
  tft.setFreeFont();
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  sprite.pushSprite(0,0);
}
//needle gauge for humidity and temperature
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

//fill buffer function
void fillBuffers() {
    // fill buffers
  while((!pm10Buffer.isFull()) || (!sumBuffer.isFull()) || (!adcBuffer.isFull())){
    getPMXdata();
    pm10Buffer.push(data.pm10);
    sumBuffer.push(data.sumBins);
    adcBuffer.push(data.xtra);
  }
}

//button handler
void btnHandler(Button2 &btn) {
  if (pthread_mutex_lock(&uiMutex) == 0) {
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
    pthread_mutex_unlock(&uiMutex);
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
          Serial.println(t.y);
          if ((t.y >= 160) && (t.y <= 320)) {
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
  disableCore0WDT();
  while (true) {
    if(!digitalRead(PIN_TOUCH_INT)) {
      if (pthread_mutex_lock(&uiMutex) == 0) {
        switchTouch();
        pthread_mutex_unlock(&uiMutex);
      }
    }
  }
}
//Check if buffer has values over 100
bool checkBuffer100(CircularBuffer<float, 11> &buffer) {
  for (uint8_t i = 0; i < buffer.size(); ++i) {
    if (buffer[i] > 100) {
      return true;
    }
  }
  return false;
}
//get max value from buffer
float maxBuffer(CircularBuffer<float, 11> &buffer) {
  float max = 0;
  for (uint8_t i = 0; i < buffer.size(); ++i) {
    if (buffer[i] > max) {
      max = buffer[i];
    }
  }
  return max;
}
//no RF24 module screen
void noRFscreen() {
  tft.fillScreen(TFT_WHITE);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.drawString("No RF24 module detected", 5, 5, 2);
  tft.drawString("Please power off and check connections", 5, 25, 2);
  tft.drawString("hot swap is not recommended", 5, 45, 2);

  for (uint8_t i=0; i < frames; ++i) {
    //iterate over frames
    delay(40);
    tft.pushImage(240, 60, animation_width, animation_height, wifiGif[i]);
  }
}