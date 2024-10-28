#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MCP23X17.h>
#include <Adafruit_SHT31.h>  // Add SHT31 library

Adafruit_MCP23X17 mcp;
Adafruit_SHT31 sht31 = Adafruit_SHT31();  // Initialize SHT31

#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64 
#define OLED_RESET -1 
#define SCREEN_ADDRESS 0x3C 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define CLK_PIN1 14 
#define DT_PIN1 13  
#define SW_PIN1 15  
#define CLK_PIN2 17 
#define DT_PIN2 16  
#define SW_PIN2 18  
#define LED1_PIN 19  
#define LED2_PIN 21  
#define DIRECTION_CW 0   
#define DIRECTION_CCW 1   
#define Motor 3

int counter1 = 1, direction1 = DIRECTION_CW, prev_CLK_state1, prevCounter1 = -1;
bool isCountingEnabled1 = false; 

int counter2 = 1, direction2 = DIRECTION_CW, prev_CLK_state2, prevCounter2 = -1;
bool isCountingEnabled2 = false;

int buttonState1, lastButtonState1 = LOW, buttonState2, lastButtonState2 = LOW;
unsigned long lastDebounceTime1 = 0, debounceDelay1 = 50, lastDebounceTime2 = 0, debounceDelay2 = 50;
int currentMode = 0;  
unsigned long mode2StartTime = 0;
bool mode2Active = false;

void setup() {
  Serial.begin(9600);
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }

  if (!mcp.begin_I2C()) {
    Serial.println("Error.");
    while (1);
  }
  for (int i = 0; i < 8; i++) {
    mcp.pinMode(i, OUTPUT);
    mcp.pinMode(i + 8, OUTPUT);
  }

  display.display();
  delay(2000);
  display.clearDisplay();

  pinMode(CLK_PIN1, INPUT);
  pinMode(DT_PIN1, INPUT);
  pinMode(SW_PIN1, INPUT_PULLUP);

  pinMode(CLK_PIN2, INPUT);
  pinMode(DT_PIN2, INPUT);
  pinMode(SW_PIN2, INPUT_PULLUP);

  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  pinMode(Motor, OUTPUT);

  prev_CLK_state1 = digitalRead(CLK_PIN1);
  prev_CLK_state2 = digitalRead(CLK_PIN2);

  if (!sht31.begin(0x44)) {  // Address of SHT31 sensor
    Serial.println("Couldn't find SHT31");
    while (1) delay(1);
  }
}

void loop() {
  debounce(SW_PIN1, lastButtonState1, buttonState1, lastDebounceTime1, debounceDelay1, handleSW1Press);
  debounce(SW_PIN2, lastButtonState2, buttonState2, lastDebounceTime2, debounceDelay2, handleSW2Press);

  if (mode2Active && millis() - mode2StartTime > 3000) {  
    Serial.println("Mode 2 timeout, returning to Mode 0");
    currentMode = 0;
    mode2Active = false;
    updateModes();
  }

  updateModes();
}

void debounce(int pin, int &lastButtonState, int &buttonState, unsigned long &lastDebounceTime, unsigned long debounceDelay, void (*pressFunction)()) {
  int reading = digitalRead(pin);
  if (reading != lastButtonState) lastDebounceTime = millis();
  if ((millis() - lastDebounceTime) > debounceDelay && reading != buttonState) {
    buttonState = reading;
    if (buttonState == LOW) pressFunction();
  }
  lastButtonState = reading;
}

void handleSW1Press() {
  if (currentMode == 0) {
    currentMode = 1;
    isCountingEnabled1 = true;
    isCountingEnabled2 = false;
  } else if (currentMode == 1) {
    currentMode = 2;
    mode2StartTime = millis();
    mode2Active = true;
  } else {
    currentMode = 0;
    mode2Active = false;
  }
  updateModes();
}

void handleSW2Press() {
  if (currentMode == 0) {
    currentMode = 3;
    isCountingEnabled2 = true;
    isCountingEnabled1 = false;
  } else if (currentMode == 3) {
    currentMode = 4;
  } else {
    currentMode = 0;
    isCountingEnabled1 = false;
    isCountingEnabled2 = false;
  }
  updateModes();
}

void updateModes() {
  float temperature = sht31.readTemperature();  // Read temperature
  if (!isnan(temperature)) {
    displayTemperature(temperature);  // Display temperature at the top left
  } else {
    Serial.println("Failed to read temperature");
  }

  switch (currentMode) {
    case 0:  
      setLEDs(LOW, LOW, 0, "Mode 0");
      mcpWrite(LOW);
      break;
    case 1:  
      setLEDs(HIGH, LOW, rotaryCounter1(), "Mode 1");
      break;
    case 2:  
      flashLED(LED1_PIN);
      setLEDs(LOW, LOW, rotaryCounter1(), "Mode 2");
      mcpSync(rotaryCounter1(), 0);
      break;
    case 3:  
      setLEDs(LOW, HIGH, rotaryCounter2(), "Mode 3");
      mcpSync(rotaryCounter2(), 8);
      break;
    case 4:  
      flashLED(LED2_PIN);
      break;
  }
}

void setLEDs(int led1State, int led2State, int counterValue, String modeText) {
  digitalWrite(LED1_PIN, led1State);
  digitalWrite(LED2_PIN, led2State);

  if (counterValue != prevCounter1 || counterValue != prevCounter2) {
    display.clearDisplay();
    display.setTextSize(1);             
    display.setTextColor(SSD1306_WHITE);        
    display.setCursor(90, 0);             
    display.print(modeText);
    
    display.setTextSize(3);
    display.setCursor(0, 40);
    display.print(map(counterValue, 1, 8, 20, 150));

    display.setTextSize(2);
    display.setCursor(60, 40);
    display.print("ppm");
    display.display();
    
    analogWrite(Motor, map(counterValue, 1, 8, 20, 150));
  }
}

void flashLED(int pin) {
  digitalWrite(pin, millis() % 500 < 250 ? HIGH : LOW);
}

void mcpSync(int counterValue, int offset) {
  for (int i = 0; i < 8; i++) {
    mcp.digitalWrite(i + offset, i < counterValue ? HIGH : LOW);
  }
}

void mcpWrite(int state) {
  for (int i = 0; i < 16; i++) mcp.digitalWrite(i, state);
}

int rotaryCounter1() {
  return rotaryCounter(CLK_PIN1, DT_PIN1, prev_CLK_state1, counter1, direction1, prevCounter1);
}

int rotaryCounter2() {
  return rotaryCounter(CLK_PIN2, DT_PIN2, prev_CLK_state2, counter2, direction2, prevCounter2);
}

int rotaryCounter(int clkPin, int dtPin, int &prevClkState, int &counter, int &direction, int &prevCounter) {
  int clkState = digitalRead(clkPin);
  if (clkState != prevClkState && clkState == HIGH) {
    if (digitalRead(dtPin) == HIGH) {
      if (counter > 1) counter--;
      direction = DIRECTION_CCW;
    } else {
      if (counter < 8) counter++;
      direction = DIRECTION_CW;
    }
  }
  prevClkState = clkState;
  return counter;
}

void displayTemperature(float temperature) {
  display.setTextSize(1);
  display.setCursor(0, 0);  // Top-left corner
  display.print("Temp: ");
  display.print(temperature);
  display.println(" C");
  display.display();
}