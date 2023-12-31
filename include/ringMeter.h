#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>


#define DEG2RAD 0.0174532925

// small needle meter 
   int   radItter;
   int   pivotNeedle_x = 30;                                                               // pivot coordinates needle of small gauge
   int   pivotNeedle_y = 150;      
   float center_x1 = 25;                                                                   // center x of edge markers circle left gauge 
   float center_y1 = 156;                                                                  // center y of edge markers circle left gauge         
   int   radius_s = 90;                                                                    // for scale markers
   int   needleLength = 80;                                                                // gauge needle length
   int   edgemarkerLength = 5;                                                             // edge marker length               
   float edge_x1, edge_y1, edge_x1_out, edge_y1_out;   
   float angleNeedle = 0;
   float needle_x, needle_y;                                                                        
   float needle_x_old, needle_y_old;
   float angleCircle = 0;
   int   pivot_x = 30;                                                                     // pivot coordinates needle of small gauge
   int   pivot_y = 150;

// ######################################################################################
// #  small needle meter       - scale creation                                         #
// ######################################################################################

void drawScaleGaugeLeft (TFT_eSPI &tft){
 
   radItter = 270;                                                                          // start point of cirle segment
   do {
       angleCircle = (radItter* DEG2RAD);                                                   // angle expressed in radians - 1 degree = 0,01745331 radians      

       edge_x1 = (center_x1+4 + (radius_s*cos (angleCircle)));                              // scale - note the 4 pixels offset in x      
       edge_y1 = (center_y1 + (radius_s*sin (angleCircle)));                                // scale
         
       edge_x1_out = (center_x1+4 + ((radius_s+edgemarkerLength)*cos (angleCircle)));       // scale - note the 4 pixels offset in x   
       edge_y1_out = (center_y1 + ((radius_s+edgemarkerLength)*sin (angleCircle)));         // scale
        
       tft.drawLine (edge_x1, edge_y1, edge_x1_out, edge_y1_out,TFT_MAGENTA); 
 
       radItter += 6; 
   } 
   while (radItter<356);                                                                    // end of circle segment
}

void drawScaleGaugeRight (TFT_eSPI &tft){
 
   radItter = 180;                                                                          // start point of cirle segment
   do {
       angleCircle = (radItter* DEG2RAD);                                                   // angle expressed in radians - 1 degree = 0,01745331 radians      

       edge_x1 = (center_x1+4 + (radius_s*cos (angleCircle)));                              // scale - note the 4 pixels offset in x      
       edge_y1 = (center_y1 + (radius_s*sin (angleCircle)));                                // scale
         
       edge_x1_out = (center_x1+4 + ((radius_s+edgemarkerLength)*cos (angleCircle)));       // scale - note the 4 pixels offset in x   
       edge_y1_out = (center_y1 + ((radius_s+edgemarkerLength)*sin (angleCircle)));         // scale
        
       tft.drawLine (edge_x1, edge_y1, edge_x1_out, edge_y1_out,TFT_MAGENTA); 
 
       radItter += 6; 
   } 
   while (radItter<266);                                                                    // end of circle segment
}


// ######################################################################################
// #  small needle meter       - dynamic needle part                                    #
// ######################################################################################

void needleMeterLeft (TFT_eSPI &tft, float &value){                                                                         

   tft.drawLine (pivotNeedle_x, pivotNeedle_y, needle_x_old, needle_y_old, 0);              // remove old needle by overwritig in white
   
   angleNeedle = (420*DEG2RAD - 1.5*round(value)*DEG2RAD);                                  // contains a 1.5 stretch factor to expand 60 percentage points over 90 degrees of scale

   if (angleNeedle > 6.28) angleNeedle = 6.28;                                              // prevents the needle from ducking below horizontal    
   needle_x = (pivotNeedle_x + ((needleLength)*cos (angleNeedle)));                         // calculate x coordinate needle point
   needle_y = (pivotNeedle_y + ((needleLength)*sin (angleNeedle)));                         // calculate y coordinate needle point
   needle_x_old = needle_x;                                                                 // remember previous needle position
   needle_y_old = needle_y;

   tft.drawLine (pivotNeedle_x, pivotNeedle_y, needle_x, needle_y,TFT_MAGENTA); 
   tft.fillCircle (pivotNeedle_x, pivotNeedle_y, 2, TFT_MAGENTA);                           // restore needle pivot
}

void needleMeterRight (TFT_eSPI &tft, float &value){                                                                         

   tft.drawLine (pivotNeedle_x, pivotNeedle_y, needle_x_old, needle_y_old, 0);              // remove old needle by overwritig in white
   
   angleNeedle = (180*DEG2RAD + 1.5*(round(value)+10)*DEG2RAD);                             // contains a 1.5 stretch factor to expand 60 percentage points over 90 degrees of scale

   if (angleNeedle < 3.14159) angleNeedle = 3.14159;                                        // prevents the needle from ducking below horizontal
   if (angleNeedle > 4.71239) angleNeedle = 4.71239;                                        // prevents the needle from ducking above vertical
   needle_x = (pivotNeedle_x + ((needleLength)*cos (angleNeedle)));                         // calculate x coordinate needle point
   needle_y = (pivotNeedle_y + ((needleLength)*sin (angleNeedle)));                         // calculate y coordinate needle point
   needle_x_old = needle_x;                                                                 // remember previous needle position
   needle_y_old = needle_y;

   tft.drawLine (pivotNeedle_x, pivotNeedle_y, needle_x, needle_y,TFT_MAGENTA); 
   tft.fillCircle (pivotNeedle_x, pivotNeedle_y, 2, TFT_MAGENTA);                           // restore needle pivot
}