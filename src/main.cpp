#include <Arduino.h>
#include <TFT_eSPI.h> 
#include <SPI.h>
#include <MAX31855soft.h>

TFT_eSPI tft = TFT_eSPI();

#define MAX_TEMP 400
#define ADC_WIN 16
#define GRIDX 130
#define GRIDY 110
#define GRIDCOLOR TFT_DARKGREY

#define HEATER 23
#define BUZZER 26
#define ENCA 12
#define ENCB 13
#define BUTTONS 35
#define GALET 34
#define HEAT_LED 25


char str[255];

typedef struct {
  int temp;
  int time; 
} thermo_t;

typedef struct  {
  thermo_t preheat;
  thermo_t soak;
  thermo_t reflow;
  thermo_t cooling;
} thermo_profile_t;

enum stage {
  PREHEAT = 1,
  SOAK,
  REFLOW,
  COOLING,
};

enum button {
  BUT_NONE = 0,
  BUT_START,
  BUT_TEMP,
  BUT_COOLING,
  BUT_CF,
};

enum galet {
  PIZZA = 0,
  CRUMPET,
  TOAST,
  BAKE,
  GRILL,
  ROST,
  COOKIES,
  REHEAT,
};

float time_div,
      temp_div;

int temp_meas = 0;
int time_now = 0;
int time_summ = 0;
thermo_profile_t select_profile;
int process_begin = 0;

uint8_t button = BUT_NONE;
uint8_t galet = 0;

double setpoint;
double input;
double output;
double kp = 300;
double ki = 0.05;
double kd = 250;

//PID reflowOvenPID(&input, &output, &setpoint, kp, ki, kd, DIRECT);
MAX31855soft myMAX31855(21, 19, 18);

thermo_profile_t Sn63Pb37 {
  { 150,  150  },  // PreHeat
  { 180,  30 },  // Soak
  { 225,  35  },  // Reflow
  { 25,   120  },  // Cooling
};



void tone(byte pin, int freq, uint16_t time) {
  ledcSetup(0, 2000, 8); // setup beeper
  ledcAttachPin(pin, 0); // attach beeper
  ledcWriteTone(0, freq); // play tone
  delay(time);
  ledcWriteTone(0, 0);
}

//Draws the grid on the display
void drawGrid(uint8_t x_start, uint8_t y_start, uint8_t cellxy) {

  for (int16_t x = x_start; x <= x_start+GRIDX; x+=cellxy) {
    tft.drawFastVLine(x, y_start, GRIDY, GRIDCOLOR); 
  }
  for (int16_t y = y_start; y <= y_start+GRIDY; y+=cellxy) {
    tft.drawFastHLine(x_start, y, GRIDX, GRIDCOLOR); 
  }
}

void drawThermoProfile(uint8_t x_start, uint8_t y_start, thermo_profile_t profile) {
  int temp_max = 0, 
      temp_min = 0;

  if(profile.preheat.temp > temp_max) temp_max = profile.preheat.temp;
  if(profile.soak.temp > temp_max) temp_max = profile.soak.temp;
  if(profile.reflow.temp > temp_max) temp_max = profile.reflow.temp;
  if(profile.cooling.temp > temp_max) temp_max = profile.cooling.temp;

  if(profile.preheat.temp < temp_min) temp_min = profile.preheat.temp;
  if(profile.soak.temp < temp_min) temp_min = profile.soak.temp;
  if(profile.reflow.temp < temp_min) temp_min = profile.reflow.temp;
  if(profile.cooling.temp < temp_min) temp_min = profile.cooling.temp;

  int temp_delta = MAX_TEMP-temp_min;
  time_summ = profile.preheat.time + profile.soak.time + profile.reflow.time + profile.cooling.time;

  time_div = (float)time_summ/GRIDX;
  temp_div = (float)temp_delta/GRIDY;

  drawGrid(x_start, y_start, 10);

  //PreHeat
  int x1 = x_start+(int)(profile.preheat.time/time_div),
      y1 = y_start+GRIDY-(int)(profile.preheat.temp/temp_div);
  tft.drawLine(x_start, y_start+GRIDY, x1, y1, TFT_GREEN);

  //Soak
  int x2 = x1+(int)(profile.soak.time/time_div),
      y2 = y_start+GRIDY-(int)(profile.soak.temp/temp_div);
  tft.drawLine(x1, y1, x2, y2, TFT_GREEN);

  //Reflow
  int x3 = x2+(int)(profile.reflow.time/time_div),
      y3 = y_start+GRIDY-(int)(profile.reflow.temp/temp_div);
  tft.drawLine(x2, y2, x3, y3, TFT_GREEN);

  //Cooling
  int x4 = x3+(int)(profile.cooling.time/time_div),
      y4 = y_start+GRIDY-(int)(profile.cooling.temp/temp_div);
  tft.drawLine(x3, y3, x4, y4, TFT_GREEN);
  
}


void ReflowProcess(void *pvParameters ){
  process_begin = 1;
  tft.fillScreen(TFT_BLACK);
  drawThermoProfile(20, 10, select_profile);

  time_summ = select_profile.preheat.time + select_profile.soak.time + select_profile.reflow.time + select_profile.cooling.time;
  time_now=0;

  thermo_t active_stage;
  int active_stage_num = 0;
  int time_to_next_stage = 0;
  int set_temp_now = 0;

  while(time_now < time_summ) {
    if(button == BUT_COOLING) {
      digitalWrite(HEATER, LOW);
      digitalWrite(HEAT_LED, HIGH);
      break;
    }
    if(time_to_next_stage) time_to_next_stage--;
    else {
      if(active_stage_num<4) active_stage_num++;
      switch(active_stage_num) {
        case PREHEAT:  active_stage = select_profile.preheat;   break;
        case SOAK:     active_stage = select_profile.soak;      break;
        case REFLOW:   active_stage = select_profile.reflow;    break;
        case COOLING:  active_stage = select_profile.cooling;   break;
      } 
      time_to_next_stage = active_stage.time;
      set_temp_now = active_stage.temp;
    }

    if(active_stage_num == COOLING) tone(BUZZER, 1000, 150);

    tft.setTextSize(1);
    tft.fillRoundRect(20, 20, 120, 14, 2, TFT_BLACK);
    sprintf(str, "TEMP:%3d  SET:%3d", temp_meas, set_temp_now);
    tft.drawString(str, 25, 25);

    tft.drawPixel(20+(time_now/time_div), 10+GRIDY-(temp_meas/temp_div), TFT_RED);

    if(temp_meas < set_temp_now) {
      digitalWrite(HEATER, HIGH);
      digitalWrite(HEAT_LED, LOW);
    }
    else {
      digitalWrite(HEATER, LOW);
      digitalWrite(HEAT_LED, HIGH);
    }
    time_now++;
    vTaskDelay(1000);
  }
  process_begin = 0;
  tft.fillScreen(TFT_BLACK);
  vTaskDelete(NULL);
}


void getTempTask(void *pvParameters ){

  int32_t rawData = 0;
  float temp=0;

  while (myMAX31855.detectThermocouple() != MAX31855_THERMOCOUPLE_OK)
  {
    switch (myMAX31855.detectThermocouple())
    {
      case MAX31855_THERMOCOUPLE_SHORT_TO_VCC:
        Serial.println(F("Thermocouple short to VCC"));
        break;

      case MAX31855_THERMOCOUPLE_SHORT_TO_GND:
        Serial.println(F("Thermocouple short to GND"));
        break;

      case MAX31855_THERMOCOUPLE_NOT_CONNECTED:
        Serial.println(F("Thermocouple not connected"));
        break;

      case MAX31855_THERMOCOUPLE_UNKNOWN:
        Serial.println(F("Thermocouple unknown error"));
        break;

      case MAX31855_THERMOCOUPLE_READ_FAIL:
        Serial.println(F("Thermocouple read error, check chip & cable"));
        break;
    }
  }

  while(1) {
    rawData = myMAX31855.readRawData();
    temp = myMAX31855.getTemperature(rawData);
    Serial.print(F("Temp: "));
    Serial.println(temp);

    temp_meas = temp;

    vTaskDelay(500);
  }
  vTaskDelete(NULL);
}


void buttonTask(void *pvParameters ){ 
  uint16_t adc_but = 0;
  uint16_t adc_galet = 0;
  while(1) {
    adc_but = analogRead(BUTTONS); 
    adc_galet = analogRead(GALET); 

    if(     adc_but < 1800  && adc_but >= 1200) button = BUT_COOLING;
    else if(adc_but < 1200  && adc_but >= 800)  button = BUT_CF;
    else if(adc_but < 800   && adc_but >= 300)  button = BUT_TEMP;
    else if(adc_but < 300)                      button = BUT_START;
    else                                        button = BUT_NONE;

    if(                           adc_galet >= 3800)   galet = REHEAT;
    else if(adc_galet < 3800  && adc_galet >= 3000)    galet = COOKIES;
    else if(adc_galet < 3000  && adc_galet >= 2500)    galet = ROST;
    else if(adc_galet < 2500  && adc_galet >= 1800)    galet = GRILL;
    else if(adc_galet < 1800  && adc_galet >= 1300)    galet = BAKE;
    else if(adc_galet < 1300   && adc_galet >= 800)    galet = TOAST;
    else if(adc_galet < 800   && adc_galet >= 300)     galet = CRUMPET;
    else if(adc_galet < 300)                           galet = PIZZA;

    if(button == BUT_START && process_begin == 0) {
      xTaskCreateUniversal(ReflowProcess, "ReflowProcess", 2048, NULL, 10, NULL,1);
    }
    vTaskDelay(100);
  }
  vTaskDelete(NULL);
}

void setup()   {
  Serial.begin(115200);

  pinMode(HEATER, OUTPUT);
  pinMode(TFT_BL, OUTPUT);
  pinMode(HEAT_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  digitalWrite(BUZZER, LOW);
  digitalWrite(HEAT_LED, HIGH);
  digitalWrite(HEATER, LOW);
  digitalWrite(TFT_BL, HIGH);

  //Set up the display
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(0, 0);

  myMAX31855.begin();
  while (myMAX31855.getChipID() != MAX31855_ID)
  {
    Serial.println(F("MAX6675 error"));
    delay(5000);
  }

  tone(BUZZER, 1000, 150);

  select_profile = Sn63Pb37;
  xTaskCreateUniversal(buttonTask, "buttonTask", 2048, NULL, 15, NULL,1);
  xTaskCreateUniversal(getTempTask, "getTempTask", 2048, NULL, 5, NULL,1);
  
  sprintf(str, "Press start");
  tft.drawString(str, 10, 50);
}

void loop() {
  if(!process_begin) {
    tft.setTextSize(2);
    tft.fillRoundRect(20, 20, 120, 30, 2, TFT_BLACK);
    sprintf(str, "TEMP:%3d C", temp_meas);
    tft.drawString(str, 25, 25);
    delay(1000);
  }
}


