#include <Wire.h>
#include "Adafruit_TCS34725.h"
#include <Adafruit_GFX.h>     // Core graphics library
#include <Adafruit_ST7735.h>  // Screen
#include <Adafruit_MPU6050.h> // Gyro
#include <Adafruit_Sensor.h>
#include <SPI.h>

#define TFT_CS        10
#define TFT_RST        9
#define TFT_DC         8

// General-use variables
float avgAccelX;
float avgAccelY;
float avgAccelZ;
long boredomWait  = 10000;
int frameRate     = 50;
uint8_t rotation  = 2;

const int button = 4;
// Screen
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
// Color Sensor
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);
byte gammatable[256]; // our RGB -> eye-recognized gamma color
// Gyro
Adafruit_MPU6050 mpu;

long      time;
long      timeSinceColorRead;
long      timeLastInteracted;
uint16_t  faceColor;
uint8_t   faceState         = 0; // Which animation the face is currently doing
/*
  0 = idle/blinking
  1 = snoozing
  2 = peekaboo
  3 = squint
  4 = screensaver
  5 = shaken
*/
uint16_t  animationStage    = 0; // Which stage of the current animation state it's in
uint8_t   stageUpdate       = 0; // "Timer" to update stage for timing-dependent animations
long      randomWait        = 0;
bool      updateIsReady     = true; // Trigger for timing-dependent animations
bool      newColorIsReady   = false; // Trigger for color update

// Variables used for screensaver
uint16_t  yVal;
uint16_t  xVal;
bool      yIsGoingUp = true;
bool      xIsGoingUp = true;

// Handle all of the gyro setup
void initGyro(){
  mpu.begin();

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  
  for (int i = 0; i < 3; i++){
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    avgAccelX += a.acceleration.x;
    avgAccelY += a.acceleration.y;
    avgAccelZ += a.acceleration.z;
    delay(500);
  }
  avgAccelX /= 3;
  avgAccelY /= 3;
  avgAccelZ /= 3;

  Serial.print("Average accelerations X: "); Serial.print(avgAccelX);
  Serial.print(", Y: "); Serial.print(avgAccelY);
  Serial.print(", Z: "); Serial.println(avgAccelZ);
}

void setup() {
  Serial.begin(9600);
  time                = millis();
  timeSinceColorRead  = millis();

  tft.initR(INITR_144GREENTAB); // Init screen ST7735R chip, green tab
  tft.fillScreen(ST77XX_BLACK);
  tft.fillCircle(64,64,54,ST77XX_RED);
  yVal = (tft.height() / 2) - 20;
  xVal = (tft.width() / 2) - 30;

  tcs.begin(); // Start color sensor
  faceColor = ST77XX_WHITE;
  tft.setRotation(2);

  // helps convert RGB colors to what humans see
  for (int i=0; i<256; i++) {
    float x = i;
    x /= 255;
    x = pow(x, 2.5);
    x *= 255;

    gammatable[i] = 255 - x;
  }
  pinMode(button, INPUT_PULLUP);
  
  initGyro();
  tft.fillScreen(ST77XX_BLACK);
}

// Reset all variables needed to go idle
void goIdle(){
  animationStage      = 0;
  stageUpdate         = 0;
  faceState           = 0;
  updateIsReady       = true;
  yVal                = (tft.height() / 2) - 20;
  xVal                = (tft.width() / 2) - 30;
  timeLastInteracted  = millis();
  tft.fillScreen(ST77XX_BLACK);
  tft.setRotation(2);
}

// function to convert rgb to hexadecimal
uint16_t rgbToHex(float r, float g, float b){
  if (r > 70 && g > 100 && b > 70) return uint16_t(0xFFFF);
  if (r+5 <= 31) r += 5;
  if (g+10 <=63) g += 10;
  if (b+5 <= 31) b += 5;
  uint16_t hexCode  = 0;
  float rbFactor    = float(31) / float(255); // 16 bit max divided into normal rgb max
  float gFactor     = float(63) / float(255);
  hexCode |= (int(round(r * rbFactor))) << 11;
  hexCode |= (int(round(g * gFactor))) << 5;
  hexCode |= (int(round(b * rbFactor)));
  // Return the equivalent hexadecimal color code
  return hexCode;
}

void updateIdle(){
  uint16_t middleX    = tft.width()/2;
  uint16_t middleY    = tft.height()/2;
  uint16_t height     = 50;
  uint16_t width      = 20;
  
  if (animationStage == 0){ // Figure out how long to wait for blink
    Serial.println("Idling");
    randomWait = random(1,40);
  }
  
  if (animationStage < randomWait * 10){ // Update eyes and eye color
    tft.fillRoundRect(middleX-(width+10), middleY-20, width, height, 10, faceColor);
    tft.fillRoundRect(middleX+10, middleY-20, width, height, 10, faceColor);
  }
  else{ // Blink
    if (animationStage == randomWait * 10){
      tft.fillScreen(ST77XX_BLACK);
    } 
    tft.fillRoundRect(middleX-(width+10), middleY, width, 10, 5, faceColor);
    tft.fillRoundRect(middleX+10, middleY, width, 10, 5, faceColor);
  }
  animationStage++;
  if (animationStage > (3 + (randomWait * 10))) animationStage = 0; // Reset blink cycle
}

void updateSleep(){
  uint16_t middleX    = tft.width()/2;
  uint16_t middleY    = tft.height()/2;
  uint16_t width      = 20;

  if (millis() - timeLastInteracted < boredomWait){
    tft.setCursor(20, 40);
    tft.setTextColor(faceColor);
    tft.setTextSize(8);
    tft.println("OO");
    if (stageUpdate == 7){
      Serial.println("Going idle");
      goIdle();
    }
    stageUpdate++;
    return;
  }

  if (animationStage == 0 || newColorIsReady){
    tft.fillRoundRect(middleX-(width+10), middleY, width, 10, 5, faceColor);
    tft.fillRoundRect(middleX+10, middleY, width, 10, 5, faceColor);
    newColorIsReady = false;
  }

  if (updateIsReady){
    int level = animationStage % 4;

    switch (level){
      case 0:
        tft.setCursor(middleX+30, 38);
        tft.setTextColor(ST77XX_WHITE);
        tft.setTextSize(1);
        tft.println("z");
        break;
      case 1:
        tft.setCursor(middleX+35, 22);
        tft.setTextColor(ST77XX_WHITE);
        tft.setTextSize(2);
        tft.println("z");
        break;
      case 2:
        tft.setCursor(middleX+40, 0);
        tft.setTextColor(ST77XX_WHITE);
        tft.setTextSize(3);
        tft.println("z");
        break;
      case 3:
        tft.fillRect(middleX+30, 0, middleX-30, 60, ST77XX_BLACK);
        break;
    }
  }

  if (stageUpdate == 5) {
    animationStage++;
    stageUpdate = 0;
    updateIsReady = true;
  }
  stageUpdate++;
}

void updatePeekaboo(){
  uint16_t  middleX   = tft.width()/2;
  uint16_t  middleY   = tft.height()/2;
  uint16_t  height    = 50;
  uint16_t  width     = 20;
  uint8_t   slideFrms = 17; // the number of frames of 5 pixels needed to slide down 84px
  uint8_t   slideUpPx = 21; // number of ppf to slide back up in 4 frames
  
  if (animationStage <= slideFrms){ // Slide down 84 pixels
    if (animationStage == 0){
      randomWait = random(10,20);
    }
    int slideFactor = animationStage * 5;
    tft.fillRect(middleX-(width+10), middleY-25+slideFactor, 60, 20, ST77XX_BLACK);
    tft.fillRoundRect(middleX-(width+10), middleY-20+slideFactor, width, height, 10, faceColor);
    tft.fillRoundRect(middleX+10, middleY-20+slideFactor, width, height, 10, faceColor);
  }
  // if peekaboo is done, pop up in 4 frames
  else if (animationStage - slideFrms >= randomWait){
    // and if we've popped up, go back to idle
    if (animationStage == slideFrms + randomWait + 10){
      goIdle();
      return;
    }
    else if (animationStage < slideFrms + randomWait + 5){
      Serial.println("Boo!");
      int slideFactor = (animationStage - (slideFrms + randomWait)) * slideUpPx;

      tft.fillRect(middleX-(width+10), 128-(slideFactor-height)-10, 60, slideUpPx+10, ST77XX_BLACK);
      tft.fillRoundRect(middleX-(width+10), 128-slideFactor, width, height, 10, faceColor);
      tft.fillRoundRect(middleX+10, 128-slideFactor, width, height, 10, faceColor);
    }
    else if (animationStage == slideFrms + randomWait + 5){
      if (stageUpdate == 0) tft.fillScreen(ST77XX_BLACK);
      tft.setCursor(middleX+35, 10);
      tft.setTextColor(ST77XX_WHITE);
      tft.setTextSize(4);
      tft.println("!");

      tft.setCursor(20, 40);
      tft.setTextColor(faceColor);
      tft.setTextSize(8);
      tft.println("^^");
    }
  }
  
  if (stageUpdate == 10) {
    animationStage++;
    stageUpdate = 0;
  }

  if (animationStage <= 20 || (animationStage - slideFrms >= randomWait)){
    animationStage++;
  }
  else stageUpdate++;
}

void updateSquint(){
  uint16_t middleX    = tft.width()/2;
  uint16_t middleY    = tft.height()/2;
  uint16_t height     = 30;
  uint16_t width      = 20;

  if (animationStage == 0){
    tft.fillScreen(ST77XX_BLACK);
    randomWait = random(5,40); // Figure out how long to wait for squinting
  }

  if (animationStage <= randomWait * 10){
    tft.fillRoundRect(middleX-(width+10), middleY, width, height, 10, faceColor);
    tft.fillRoundRect(middleX+10, middleY, width, height, 10, faceColor);
    tft.fillRoundRect(middleX-(width+15), middleY, height, 10, 5, faceColor);
    tft.fillRoundRect(middleX+5, middleY, height, 10, 5, faceColor);
  }
  else{
    Serial.println("...Going idle!");
    goIdle();
    return;
  }
  animationStage++;
}

void updateScreensaver(){
  uint16_t height = 50;
  uint16_t width  = 20;
  int maxX        = tft.width() - 60;
  int maxY        = tft.height() - height;

  if (millis() - timeLastInteracted < boredomWait){
    tft.setCursor(20, 40);
    tft.setTextColor(faceColor);
    tft.setTextSize(8);
    tft.println("OO");
    if (stageUpdate == 7){
      Serial.println("Going idle");
      goIdle();
    }
    stageUpdate++;
    faceColor = ST77XX_WHITE;
    return;
  }

  if (updateIsReady){
    tft.fillRoundRect(xVal, yVal, width, height, 10, faceColor);
    tft.fillRoundRect(xVal+40, yVal, width, height, 10, faceColor);
    // Adjust X direction if needed and slide over a bit more
    if (xIsGoingUp && xVal <= maxX - 5) xVal += 5;
    else if (xIsGoingUp){
      xIsGoingUp = false;
      xVal -= 5;
    }
    else if (!xIsGoingUp && xVal >= 5) xVal -= 5;
    else{
      xIsGoingUp = true;
      xVal += 5;
    }


    // Adjust Y direction if needed and slide over a bit more
    if (yIsGoingUp && yVal <= maxY - 5) yVal += 5;
    else if (yIsGoingUp){
      yIsGoingUp = false;
      yVal -= 5;
    }
    else if (!yIsGoingUp && yVal >= 5) yVal -= 5;
    else{
      yIsGoingUp = true;
      yVal += 5;
    }
    
    updateIsReady = false;
  }

  if (stageUpdate == 2) {
    animationStage++;
    stageUpdate   = 0;
    updateIsReady = true;
    faceColor < (0xFFFF-100) ? faceColor += 100 : faceColor = 0;
  }
  stageUpdate++;
}

void updateShaken(){
  uint16_t middleX    = tft.width()/2;
  uint16_t middleY    = tft.height()/2;

  if (animationStage == 0) updateIsReady = true;

  if (updateIsReady){
    tft.setRotation(animationStage % 4);
    tft.setCursor(20, 60);
    tft.setTextColor(faceColor);
    tft.setTextSize(8);
    tft.println("><");
  }

  if (stageUpdate == 2) {
    animationStage++;
    stageUpdate = 0;
    updateIsReady = true;
    tft.fillScreen(ST77XX_BLACK);
  }
  stageUpdate++;
  if (animationStage > 10) goIdle();
}

void updateFace(){
  switch (faceState){
    case 0:
      updateIdle();
      break;
    case 1:
      updateSleep();
      break;
    case 2:
      updatePeekaboo();
      break;
    case 3:
      updateSquint();
      break;
    case 4:
      updateScreensaver();
      break;
    case 5:
      updateShaken();
      break;
  }
}

void loop() {
  float red, green, blue;
  long currTime = millis();
  // If it's been awhile since the user interacted with it, go into boredom
  if (faceState == 0 && currTime - timeLastInteracted > boredomWait) {
    Serial.println("I'm bored!");
    faceState       = random(1,5);
    animationStage  = 0;
    stageUpdate     = 0;
    if (faceState == 1) tft.fillScreen(ST77XX_BLACK);
  }

  // If we've hit our framerate update time, update the face
  if (currTime - time >= frameRate) {
    updateFace();
    time = currTime;
  }

  /* Get new gyro sensor events with the readings */
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  bool diffX = abs(avgAccelX - a.acceleration.x) > 1.00;
  bool diffY = abs(avgAccelY - a.acceleration.y) > 1.00;
  bool diffZ = abs(avgAccelZ - a.acceleration.z) > 0.50;
  
  if (!(faceState == 2) && !(faceState == 5) && (diffX && diffY && diffZ)){
    Serial.println("SHAKE DETECTED");
    timeLastInteracted = millis();
    faceState = 5;
    animationStage  = 0;
    stageUpdate     = 0;
    tft.fillScreen(ST77XX_BLACK);
  }


  int buttonCheck = digitalRead(button);
  /*
  delay(20);
  if (digitalRead(button) == 0) buttonCheck = 0;
  delay(20);
  if (digitalRead(button) == 0) buttonCheck = 0;
  delay(10);
  if (digitalRead(button) == 0) buttonCheck = 0;
  delay(10);
*/

  if (buttonCheck == LOW && (currTime - timeSinceColorRead > 500)){
    Serial.print("Reading color...buttonCheck is "); Serial.println(buttonCheck);
    tcs.setInterrupt(false);  // turn on LED

    delay(80);

    tcs.getRGB(&red, &green, &blue);
    tcs.setInterrupt(true);  // turn off LED
    faceColor = rgbToHex(red+5, green+10, blue+5);
    Serial.print("FaceColor is: ");
    Serial.println(faceColor, HEX);
    timeSinceColorRead = millis();
    timeLastInteracted = millis();
    newColorIsReady = true;
    if (faceState == 1 || faceState == 4) {
      tft.fillScreen(ST77XX_BLACK);
      stageUpdate = 0; // Make sure sleep is ready to pop awake
    }
  }
  

}
