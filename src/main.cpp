#include "main.h"

#define HEATER 23
#define BUZZER 26
#define ENCA 12
#define ENCB 13
#define BUTTONS 35
#define GALET 34
#define HEAT_LED 25
#define FAN 0

#define DEBUG

enum buttons {
  BUT_NONE = 0,
  BUT_START,
  BUT_TEMP,
  BUT_COOLING,
  BUT_CF,
}button;

enum galets {
  PIZZA = 0,
  CRUMPET,
  TOAST,
  BAKES,
  GRILL,
  ROST,
  COOKIES,
  REHEAT,
}galet;

uint8_t galet_old = 0;

uint8_t UM_Logo[1];

// Save data struct
typedef struct {
  int valid = false;
  int useFan = false;
  int fanTimeAfterReflow = 60;
  int paste = 0;
  float power = 1;
  int lookAhead = 6;
  int lookAheadWarm = 1;
  int tempOffset = 0;
  long bakeTime = 1200; // 20 mins
  float bakeTemp = 45; // Degrees C
  int bakeTempGap = 3; // Aim for the desiTFT_RED temp minus this value to compensate for overrun
  int startFullBlast = false;
  int beep = true;
} Settings;

// UI and runtime states
enum states {
  BOOT = 0,
  WARMUP = 1,
  REFLOW = 2,
  FINISHED = 3,
  MENU = 10,
  SETTINGS = 11,
  SETTINGS_PASTE = 12,
  SETTINGS_RESET = 13,
  OVENCHECK = 15,
  OVENCHECK_START = 16,
  BAKE_MENU = 20,
  BAKE = 21,
  BAKE_DONE = 22,
  ABORT = 99
} state;

enum enc_modes {
  BAKE_MENU_TEMP = 0,
  BAKE_MENU_TIME = 1,


} enc_mode;

const String ver = "2.02";
bool newSettings = false;

const int heater_ch = 0;
const int buzzer_ch = 1;
// TC variables
unsigned long nextTempRead = 0;
unsigned long nextTempAvgRead = 0;
int avgReadCount = 0;

unsigned long keepFanOnTime = 0;

double timeX = 0;
double tempOffset = 60;

// Bake variables
long currentBakeTime = 0; // Used to countdown the bake time
byte currentBakeTimeCounter = 0;
int lastTempDirection = 0;
long minBakeTime = 600; // 10 mins in seconds
long maxBakeTime = 10800; // 3 hours in seconds
float minBakeTemp = 45; // 45 Degrees C
float maxBakeTemp = 100; // 100 Degrees C

// Current index in the settings screen
int settings_pointer = 0;

// Initialise an array to hold 5 profiles
// Increase this array if you plan to add more
ReflowGraph solderPaste[5];
// Index into the current profile
int currentGraphIndex = 0;

// Calibration data - currently diabled in this version
int calibrationState = 0;
long calibrationSeconds = 0;
long calibrationStatsUp = 0;
long calibrationStatsDown = 300;
bool calibrationUpMatch = false;
bool calibrationDownMatch = false;
float calibrationDropVal = 0;
float calibrationRiseVal = 0;

// Runtime reflow variables
int tcError = 0;
bool tcWasError = false;
bool tcWasGood = true;
float currentDuty = 0;
float currentTemp = 0;
float prevTemp = 0;
float cachedCurrentTemp = 0;
float currentTempAvg = 0;
float lastTemp = -1;
float currentDetla = 0;
unsigned int currentPlotColor = TFT_GREEN;
bool isCuttoff = false;
bool isFanOn = false;
float lastWantedTemp = -1;
int buzzerCount = 5;

// Graph Size for UI
int graphRangeMin_X = 0;
int graphRangeMin_Y = 30;
int graphRangeMax_X = -1;
int graphRangeMax_Y = 110;
int graphRangeStep_X = 45;
int graphRangeStep_Y = 20;

// UI button positions and sizes
int buttonPosY[] = { 19, 74, 129, 184 };
int buttonHeight = 16;
int buttonWidth = 4;


// Create a spline reference for converting profile values to a spline for the graph
Spline baseCurve;

// Initiliase a reference for the settings file that we store in flash storage
Settings set;


TFT_eSPI tft = TFT_eSPI();
MAX31855soft tc(21, 19, 18);
Encoder enc(ENCB, ENCA);


// Debug printing functions
void debug_print(String txt)
{
#ifdef DEBUG
  Serial.print(txt);
#endif
}

void debug_print(int txt)
{
#ifdef DEBUG
  Serial.print(txt);
#endif
}

void debug_println(String txt)
{
#ifdef DEBUG
  Serial.println(txt);
#endif
}

void debug_println(int txt)
{
#ifdef DEBUG
  Serial.println(txt);
#endif
}

Settings NVS_store_read(){
  Settings nvs_set;

  nvs_set.valid = NVS.getInt("valid");
  nvs_set.useFan = NVS.getInt("useFan");
  nvs_set.fanTimeAfterReflow = (byte)NVS.getInt("fanTimeAfterReflow");
  nvs_set.paste = NVS.getInt("paste");
  nvs_set.power = NVS.getFloat("power");
  nvs_set.lookAhead = NVS.getInt("lookAhead");
  nvs_set.lookAheadWarm = NVS.getInt("lookAheadWarm");
  nvs_set.tempOffset = NVS.getInt("tempOffset");
  nvs_set.tempOffset = NVS.getInt("tempOffset");
  nvs_set.bakeTemp = NVS.getFloat("bakeTemp");
  nvs_set.bakeTempGap = NVS.getInt("bakeTempGap");
  nvs_set.startFullBlast = NVS.getInt("startFullBlast");
  nvs_set.beep = NVS.getInt("beep");

  return nvs_set;
}

void NVS_store_write(Settings nvs_set) {
  int ok;

  ok += NVS.setInt("valid", nvs_set.valid);
  ok += NVS.setInt("useFan", nvs_set.useFan);
  ok += NVS.setInt("fanTimeAfterReflow", nvs_set.fanTimeAfterReflow);
  ok += NVS.setInt("paste", nvs_set.paste);
  ok += NVS.setFloat("power", nvs_set.power);
  ok += NVS.setInt("lookAhead", nvs_set.lookAhead);
  ok += NVS.setInt("lookAheadWarm", nvs_set.lookAheadWarm);
  ok += NVS.setInt("tempOffset", nvs_set.tempOffset);
  ok += NVS.setInt("tempOffset", nvs_set.tempOffset);
  ok += NVS.setFloat("bakeTemp", nvs_set.bakeTemp);
  ok += NVS.setInt("bakeTempGap", nvs_set.bakeTempGap);
  ok += NVS.setInt("startFullBlast", nvs_set.startFullBlast);
  ok += NVS.setInt("beep", nvs_set.beep);
  
  if(ok == 0) debug_println("NVS Read OK");
}


void tone(byte pin, int freq, uint16_t time) {
  ledcSetup(buzzer_ch, 2000, 8); // setup beeper
  ledcAttachPin(pin, buzzer_ch); // attach beeper
  ledcWriteTone(buzzer_ch, freq); // play tone
  delay(time);
  ledcWriteTone(buzzer_ch, 0);
}

void noTone() {
  ledcWriteTone(buzzer_ch, 0);
}




void LoadPaste()
{
  /*
    Each profile is initialised with the follow data:

        Paste name ( String )
        Paste type ( String )
        Paste Reflow Temperature ( int )
        Profile graph X values - time
        Profile graph Y values - temperature
        Length of the graph  ( int, how long if the graph array )
  */
  float baseGraphX[7] = { 1, 90, 180, 210, 240, 270, 300 }; // time
  float baseGraphY[7] = { 30, 90, 130, 138, 165, 138, 27 }; // value
  solderPaste[0] = ReflowGraph( "CHIPQUIK", "No-Clean Sn42/Bi57.6/Ag0.4", 138, baseGraphX, baseGraphY, ELEMENTS(baseGraphX) );

  float baseGraphX1[7] = { 1,  90, 180,  225, 240, 270, 300 }; // time
  float baseGraphY1[7] = { 25, 150, 175, 190, 210, 125, 50 }; // value
  solderPaste[1] = ReflowGraph( "CHEMTOOLS L", "No Clean 63CR218 Sn63/Pb37", 183, baseGraphX1, baseGraphY1, ELEMENTS(baseGraphX1) );

  float baseGraphX2[7] = { 1,  120, 240, 270, 300, 330, 360 }; // time
  float baseGraphY2[7] = { 25, 150, 150, 185, 220, 160, 20 }; // value
  solderPaste[2] = ReflowGraph( "Mechanic XG-50 Lite", "No Clean Sn63/Pb36", 183, baseGraphX2, baseGraphY2, ELEMENTS(baseGraphX2) );

  float baseGraphX3[6] = { 1,  90,  210, 310, 340, 370 }; // time
  float baseGraphY3[6] = { 25, 150, 150, 235, 160, 20 }; // value
  solderPaste[3] = ReflowGraph( "Mechanic XG-50", "No Clean Sn63/Pb36", 183, baseGraphX3, baseGraphY3, ELEMENTS(baseGraphX3) );

  float baseGraphX4[6] = { 1,  90,  210, 225, 330, 360 }; // time
  float baseGraphY4[6] = { 25, 150, 150, 220, 100, 25 }; // value
  solderPaste[4] = ReflowGraph( "CHEMTOOLS SAC305 HD", "Sn96.5/Ag3.0/Cu0.5", 225, baseGraphX4, baseGraphY4, ELEMENTS(baseGraphX4) );

  //TODO: Think of a better way to initalise these baseGraph arrays to not need unique array creation
}

// Obtain the temp value of the current profile at time X
int GetGraphValue( int x )
{
  return ( solderPaste[currentGraphIndex].reflowTemp[x] );
}

int GetGraphTime( int x )
{
  return ( solderPaste[currentGraphIndex].reflowTime[x] );
}


// Set the current profile via the array index
void SetCurrentGraph( int index )
{
  currentGraphIndex = index;
  graphRangeMax_X = solderPaste[currentGraphIndex].MaxTime();
  graphRangeMax_Y = solderPaste[currentGraphIndex].MaxTempValue() + 5; // extra padding
  graphRangeMin_Y = solderPaste[currentGraphIndex].MinTempValue();

  debug_print("Setting Paste: ");
  debug_println( solderPaste[currentGraphIndex].n );
  debug_println( solderPaste[currentGraphIndex].t );

  // Initialise the spline for the profile to allow for smooth graph display on UI
  timeX = 0;
  baseCurve.setPoints(solderPaste[currentGraphIndex].reflowTime, solderPaste[currentGraphIndex].reflowTemp, solderPaste[currentGraphIndex].reflowTangents, solderPaste[currentGraphIndex].len);
  baseCurve.setDegree( Hermite );

  // Re-interpolate data based on spline
  for ( int ii = 0; ii <= graphRangeMax_X; ii += 1 )
  {
    solderPaste[ currentGraphIndex ].wantedCurve[ii] = baseCurve.value(ii);
  }

  // calculate the biggest graph movement delta
  float lastWanted = -1;
  for ( int i = 0; i < solderPaste[ currentGraphIndex ].offTime; i++ )
  {
    float wantedTemp = solderPaste[ currentGraphIndex ].wantedCurve[ i ];

    if ( lastWanted > -1 )
    {
      float wantedDiff = (wantedTemp - lastWanted );

      if ( wantedDiff > solderPaste[ currentGraphIndex ].maxWantedDelta )
        solderPaste[ currentGraphIndex ].maxWantedDelta = wantedDiff;
    }
    lastWanted  = wantedTemp;
  }
}

void setup()
{
  NVS.begin();
  // Setup all GPIO
  pinMode(HEATER, OUTPUT);
  pinMode(TFT_BL, OUTPUT);
  pinMode(HEAT_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);
  digitalWrite(HEAT_LED, HIGH);
  digitalWrite(HEATER, LOW);
  digitalWrite(TFT_BL, HIGH);

  ledcSetup(heater_ch, 5000, 10);
  ledcAttachPin(HEATER, heater_ch);

  SetRelayFrequency( 0 ); // Turn off the SSR - duty cycle of 0

  enc.setTickMode(AUTO);
  xTaskCreateUniversal(buttonTask, "buttonTask", 2048, NULL, 15, NULL,1);

#ifdef DEBUG
  Serial.begin(115200);
#endif

  // just wait a bit before we try to load settings from FLASH
  delay(500);

  // load settings from FLASH
  set = NVS_store_read();
  
  // If no settings were loaded, initialise data and save
  if ( !set.valid )
  {
    SetDefaults();
    newSettings = true;
    NVS_store_write(set);
  }

  debug_println("TFT Begin...");

  // Start up the TFT and show the boot screen
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(0, 0);
  
  //BootScreen();
  BuzzerStart();

  delay(200);

  // Start up the MAX31855
  debug_println("Thermocouple Begin...");
  tc.begin();

  // delay for initial temp probe read to be garbage
  delay(500);

  // Load up the profiles
  LoadPaste();
  // Set the current profile based on last selected
  SetCurrentGraph( set.paste );

  delay(500);

  // Show the main menu
  ShowMenu();
}

// Helper method to display the temperature on the TFT
void DisplayTemp( bool center)
{
  if ( center )
  {
    tft.setTextColor( TFT_YELLOW, TFT_BLACK );
    tft.setTextSize(3); //2
    println_Center("  " + String( round( currentTemp ) , 1) + "c  ", tft.width() / 2, ( tft.height() / 2 ) + 10 );
  }
  else
  {
    if ( round( currentTemp ) != round( cachedCurrentTemp ) )
    {
      // display the previous temp in TFT_BLACK to clear it
      tft.setTextColor( TFT_BLACK, TFT_BLACK );
      tft.setTextSize(3); //2
      tft.setCursor( 10,  ( tft.height() / 2 ) - 10 );
      tft.println( String( round( cachedCurrentTemp ), 1) + "c");

      // display the new temp in TFT_GREEN
      tft.setTextColor( TFT_GREEN, TFT_BLACK );
      tft.setCursor( 10,  ( tft.height() / 2 ) - 10 );
      tft.println( String( round( currentTemp ), 1 ) + "c" );

      // cache the current temp
      cachedCurrentTemp = currentTemp;
    }
  }
}

void loop()
{

  // Current activity state machine
  if ( state == BOOT ) // BOOT
  {
    return;
  }
  else if ( state == WARMUP ) // WARMUP - We sit here until the probe reaches the starting temp for the profile
  {
    if ( nextTempRead < millis() ) // we only read the probe every second
    {
      nextTempRead = millis() + 1000;

      ReadCurrentTemp();
      MatchTemp();

      if ( currentTemp >= GetGraphValue(0) )
      {
        // We have reached the starting temp for the profile, so lets start baking our boards!
        lastTemp = currentTemp;
        avgReadCount = 0;
        currentTempAvg = 0;

        StartReflow();
      }
      else if ( currentTemp > 0 )
      {
        // Show the current probe temp so we can watch as it reaches the minimum starting value
        DisplayTemp( true );
      }
    }
    return;
  }
  else if ( state == FINISHED ) // FINISHED
  {
    // do we keep the fan on after reflow finishes to help cooldown?
    KeepFanOnCheck();
    return;
  }
  else if ( state == SETTINGS || state == SETTINGS_PASTE || state == SETTINGS_RESET ) // SETTINGS
  {
    // Currently not used
    return;
  }
  else if ( state == MENU ) // MENU
  {
    if ( nextTempRead < millis() )
    {
      nextTempRead = millis() + 1000;

      // We show the current probe temp in the men screen just for info
      ReadCurrentTemp();

      if ( tcError > 0 )
      {
        // Clear TC Temp background if there was one!
        if ( tcWasGood )
        {
          tcWasGood = false;
          tft.setTextColor( TFT_BLACK, TFT_BLACK );
          tft.setTextSize(3); //2
          tft.setCursor( 10,  ( tft.height() / 2 ) - 10 );
          tft.println( String( round( cachedCurrentTemp ), 1 ) + "c" );

          tft.fillRect( 5, tft.height() / 2 - 20, 150, 30, TFT_RED );
        }
        tcWasError = true;
        tft.setTextColor( TFT_WHITE, TFT_RED );
        tft.setTextSize(2);        
        tft.setCursor( 10, ( tft.height() / 2 ) - 20 );
        tft.println( "TC ERR #" + String( tcError ) );
      }
      else if ( currentTemp > 0 )
      {
        // Clear error background if there was one!
        if ( tcWasError )
        {
          cachedCurrentTemp = 0;
          tcWasError = false;
          tft.fillRect( 0, tft.height() / 2 - 20, 160, 30, TFT_BLACK );
        }
        tcWasGood = true;
        DisplayTemp(false);
      }
    }

    // do we keep the fan on after reflow finishes to help cooldown?
    KeepFanOnCheck();
    return;
  }
  else if ( state == BAKE_MENU || state == BAKE_DONE )
  {
    // do nothing in these states
  }
  else if ( state == BAKE )
  {
    if ( currentBakeTime > 0 )
    {
      if ( nextTempAvgRead < millis() )
      {
        nextTempAvgRead = millis() + 100;
        ReadCurrentTempAvg();
      }

      if ( nextTempRead < millis() )
      {
        nextTempRead = millis() + 1000;

        // Set the temp from the average
        currentTemp = ( currentTempAvg / avgReadCount );
        // clear the variables for next run
        avgReadCount = 0;
        currentTempAvg = 0;

        // Control the SSR
        MatchTemp_Bake();

        if ( currentTemp > 0 )
          currentBakeTime--;
      }

      UpdateBake();
    }
    else
    {
      BakeDone();
    }
  }
  else if ( state == OVENCHECK )
  {
    // Currently not used
    return;
  }
  else if ( state == OVENCHECK_START ) // calibration - not currently used
  {
    if ( nextTempRead < millis() )
    {
      nextTempRead = millis() + 1000;

      ReadCurrentTemp();

      MatchCalibrationTemp();

      if ( calibrationState < 2 )
      {
        tft.setTextColor( TFT_CYAN, TFT_BLACK );
        tft.setTextSize(1);

        if ( calibrationState == 0 )
        {
          if ( currentTemp < GetGraphValue(0) )
            println_Center("WARMING UP", tft.width() / 2, ( tft.height() / 2 ) );
          else
            println_Center("HEAT UP SPEED", tft.width() / 2, ( tft.height() / 2 ) );
          tft.setTextSize(1);
          println_Center("TARGET " + String( GetGraphValue(1) ) + "c in " + String( GetGraphTime(1) ) + "s", tft.width() / 2, ( ( tft.height()/2 ) + 30 ) );
        }
        else if ( calibrationState == 1 )
        {
          println_Center("COOL DOWN LEVEL", tft.width() / 2, ( tft.height() / 2 ) );
          tft.fillRect( 0, tft.height() - 30, tft.width(), 30, TFT_BLACK );
        }

        // only show the timer when we have hit the profile starting temp
        if (currentTemp >= GetGraphValue(0) )
        {
          // adjust the timer colour based on good or bad values
          if ( calibrationState == 0 )
          {
            if ( calibrationSeconds <= GetGraphTime(1) )
              tft.setTextColor( TFT_WHITE, TFT_BLACK );
            else
              tft.setTextColor( TFT_ORANGE, TFT_BLACK );
          }
          else
          {
            tft.setTextColor( TFT_WHITE, TFT_BLACK );
          }

          tft.setTextSize(2); //2
          println_Center(" " + String( calibrationSeconds ) + " secs ", tft.width() / 2, ( tft.height() / 2 ) + 15 );
        }
        tft.setTextSize(2); //2
        tft.setTextColor( TFT_YELLOW, TFT_BLACK );
        println_Center(" " + String( round( currentTemp ), 1 ) + "c ", tft.width() / 2, ( tft.height() / 2 ) + 50 );


      }
      else if ( calibrationState == 2 )
      {
        calibrationState = 3;

        tft.setTextColor( TFT_GREEN, TFT_BLACK );
        tft.setTextSize(2);
        tft.fillRect( 0, (tft.height() / 2 ) - 35, tft.width(), (tft.height() / 2 ) + 45, TFT_BLACK );
        println_Center("RESULTS!", tft.width() / 2, ( tft.height() / 2 ) - 20 );

        tft.setTextSize(1);
        tft.setTextColor( TFT_WHITE, TFT_BLACK );
        tft.setCursor( 10, ( tft.height() / 2 ) );
        tft.print( "RISE " );
        if ( calibrationUpMatch )
        {
          tft.setTextColor( TFT_GREEN, TFT_BLACK );
          tft.print( "PASS" );
        }
        else
        {
          tft.setTextColor( TFT_ORANGE, TFT_BLACK );
          tft.print( "FAIL " );
          tft.setTextColor( TFT_WHITE, TFT_BLACK );
          tft.print( "REACHED " + String( round(calibrationRiseVal * 100) ) + "%") ;
        }

        tft.setTextColor( TFT_WHITE, TFT_BLACK );
        tft.setCursor( 10, ( tft.height() / 2 ) + 10 );
        tft.print( "DROP " );
        if ( calibrationDownMatch )
        {
          tft.setTextColor( TFT_GREEN, TFT_BLACK );
          tft.print( "PASS" );
          tft.setTextColor( TFT_WHITE, TFT_BLACK );
          tft.print( "DROPPED " + String( round(calibrationDropVal * 100) ) + "%") ;
        }
        else
        {
          tft.setTextColor( TFT_ORANGE, TFT_BLACK );
          tft.print( "FAIL " );
          tft.setTextColor( TFT_WHITE, TFT_BLACK );
          tft.print( "DROPPED " + String( round(calibrationDropVal * 100) ) + "%") ;

          tft.setTextColor( TFT_WHITE, TFT_BLACK );
          tft.setCursor( 10, ( tft.height() / 2 ) + 20 );
          tft.print( "RECOMMEND ADDING FAN") ;
        }
      }
    }
  }
  else // state is REFLOW
  {
    if ( nextTempAvgRead < millis() )
    {
      nextTempAvgRead = millis() + 100;
      ReadCurrentTempAvg();
    }
    if ( nextTempRead < millis() )
    {
      nextTempRead = millis() + 1000;

      // Set the temp from the average
      currentTemp = ( currentTempAvg / avgReadCount );
      // clear the variables for next run
      avgReadCount = 0;
      currentTempAvg = 0;

      // Control the SSR
      MatchTemp();

      if ( currentTemp > 0 )
      {
        timeX++;

        if ( timeX > solderPaste[currentGraphIndex].completeTime )
        {
          EndReflow();
        }
        else
        {
          Graph(timeX, currentTemp, 20, 118, 140, 98 );

          if ( timeX < solderPaste[currentGraphIndex].fanTime )
          {
            float wantedTemp = solderPaste[currentGraphIndex].wantedCurve[ (int)timeX ];
            DrawHeading( String( round( currentTemp ), 0 ) + "/" + String( (int)wantedTemp ) + "c", currentPlotColor, TFT_BLACK );
          }
        }
      }
    }
  }
}

// This is where the SSR is controlled via PWM
void SetRelayFrequency( int duty )
{
  // calculate the wanted duty based on settings power override
  currentDuty = ((float)duty * set.power  * 4);

  // Write the clamped duty cycle to the RELAY GPIO
  ledcWrite(heater_ch, constrain( round( currentDuty ), 0, 1024));
  
  debug_print("RELAY Duty Cycle: ");
  debug_println( String( ( currentDuty / 1024.0 ) * 100) + "%" + " Using Settings Power: " + String( round( set.power * 100 )) + "%" );
}

/*
   SOME CALIBRATION CODE THAT IS CURRENTLY USED FOR THE OVEN CHECK SYSTEM
   Oven Check currently shows you hoe fast your oven can reach the initial pre-soak temp for your selected profile
*/

void MatchCalibrationTemp()
{
  if ( calibrationState == 0 ) // temp speed
  {
    // Set SSR to full duty cycle - 100%
    SetRelayFrequency( 255 );

    // Only count seconds from when we reach the profile starting temp
    if ( currentTemp >= GetGraphValue(0) )
      calibrationSeconds ++;

    if ( currentTemp >= GetGraphValue(1) )
    {
      debug_println("Cal Heat Up Speed " + String( calibrationSeconds ) );

      calibrationRiseVal =  ( (float)currentTemp / (float)( GetGraphValue(1) ) );
      calibrationUpMatch = ( calibrationSeconds <= GetGraphTime(1) );
      calibrationState = 1; // cooldown
      SetRelayFrequency( 0 );
      StartFan( false );
      Buzzer( 2000, 50 );
    }
  }
  else if ( calibrationState == 1 )
  {
    calibrationSeconds --;
    SetRelayFrequency( 0 );

    if ( calibrationSeconds <= 0 )
    {
      Buzzer( 2000, 50 );
      debug_println("Cal Cool Down Temp " + String( currentTemp ) );

      // calc calibration drop percentage value
      calibrationDropVal = ( (float)( GetGraphValue(1) - currentTemp ) / (float)GetGraphValue(1) );
      // Did we drop in temp > 33% of our target temp? If not, recomment using a fan!
      calibrationDownMatch = ( calibrationDropVal > 0.33 );

      calibrationState = 2; // finished
      StartFan( true );
    }
  }
}

/*
   END
   SOME CALIBRATION CODE THAT IS CURRENTLY USED FOR THE OVEN CHECK SYSTEM
*/

void KeepFanOnCheck()
{
  // do we keep the fan on after reflow finishes to help cooldown?
  if ( set.useFan && millis() < keepFanOnTime )
    StartFan( true );
  else
    StartFan( false );
}

void ReadCurrentTempAvg()
{
  int32_t rawData = 0;

  rawData = tc.readRawData();
  float curTemp = tc.getTemperature(rawData);
  //-------Fix отвала термопары 06.12.21--------
  if(curTemp == MAX31855_ERROR){
    tcError = 1;
    currentTempAvg += prevTemp + set.tempOffset;
  }
  else {
    tcError = 0;
    prevTemp = curTemp;
    currentTempAvg += curTemp + set.tempOffset;
  }
  //-------Fix_end--------

  //tcError = (curTemp == MAX31855_ERROR) ? 1 : 0;
  //currentTempAvg += curTemp + set.tempOffset;
  avgReadCount++;
}

// Read the temp probe
void ReadCurrentTemp()
{
  int32_t rawData = 0;

  rawData = tc.readRawData();
  float curTemp = tc.getTemperature(rawData);
  //tcError = (curTemp == MAX31855_ERROR) ? 1 : 0;

  //-------Fix отвала термопары 06.12.21--------
  if(curTemp == MAX31855_ERROR){
    tcError = 1;
    currentTemp = prevTemp + set.tempOffset;
  }
  else {
    tcError = 0;
    prevTemp = curTemp;
    currentTemp = curTemp + set.tempOffset;  
  }
  //-------Fix_end--------

  //currentTemp = curTemp + set.tempOffset;
  currentTemp =  constrain(currentTemp, -10, 350);

  debug_print("TC Read: ");
  debug_println( currentTemp );
}

void MatchTemp_Bake()
{
  float duty = 0;
  float tempDiff = 0;
  float perc = 0;

  float wantedTemp = set.bakeTemp - set.bakeTempGap;

  // If the last temperature direction change is dropping, we want to aim for the bake temp without the gap
  if ( currentTemp < set.bakeTemp && lastTempDirection < 0 )
    wantedTemp = set.bakeTemp + 1; // We need to ramp a little more as we've likely been off for a while relying on thermal mass to hold temp

  debug_print( "Bake T: " );
  debug_print( currentBakeTime);
  debug_print( "  Current: " );
  debug_print( currentTemp );
  debug_print( "  LastDir: " );
  debug_print( lastTempDirection );

  if ( currentTemp >= wantedTemp)
  {
    duty = 0;
    perc = 0;

    if ( set.useFan )
    {
      if (currentTemp > set.bakeTemp + 3)
        StartFan( true );
      else
        StartFan( false );
    }
  }
  else
  {
    // if current temp is less thn half of wanted, then boost at 100%
    if ( currentTemp < ( wantedTemp / 2 ) )
      perc = 1;
    else
    {
      tempDiff = wantedTemp - currentTemp;
      perc = tempDiff / wantedTemp;
    }

    duty = 255 * perc;
  }

  debug_print( "  Perc: " );
  debug_print( perc );
  debug_print( "  Duty: " );
  debug_print( duty );
  debug_print( "  " );

  duty = constrain( duty, 0, 255 );
  currentPlotColor = TFT_GREEN;
  SetRelayFrequency( duty );
}


// This is where the magic happens for temperature matching
void MatchTemp()
{
  float duty = 0;
  float wantedTemp = 0;
  float wantedDiff = 0;
  float tempDiff = 0;
  float perc = 0;

  // if we are still before the main flow cut-off time (last peak)
  if ( timeX < solderPaste[currentGraphIndex].offTime )
  {
    // We are looking XXX steps ahead of the ideal graph to compensate for slow movement of oven temp
    if ( timeX < solderPaste[currentGraphIndex].reflowTime[2] )
      wantedTemp = solderPaste[currentGraphIndex].wantedCurve[ (int)timeX + set.lookAheadWarm ];
    else
      wantedTemp = solderPaste[currentGraphIndex].wantedCurve[ (int)timeX + set.lookAhead ];

    wantedDiff = (wantedTemp - lastWantedTemp );
    lastWantedTemp = wantedTemp;

    tempDiff = ( currentTemp - lastTemp );
    lastTemp = currentTemp;

    perc = wantedDiff - tempDiff;

    debug_print( "T: " );
    debug_print( timeX );

    debug_print( "  Current: " );
    debug_print( currentTemp );

    debug_print( "  Wanted: " );
    debug_print( wantedTemp );

    debug_print( "  T Diff: " );
    debug_print( tempDiff  );

    debug_print( "  W Diff: " );
    debug_print( wantedDiff );

    debug_print( "  Perc: " );
    debug_print( perc );

    isCuttoff = false;

    // have to passed the fan turn on time?
    if ( !isFanOn && timeX >= solderPaste[currentGraphIndex].fanTime )
    {
      debug_print( "COOLDOWN: " );

      // If we are usng the fan, turn it on
      if ( set.useFan )
      {
        DrawHeading( "COOLDOWN!", TFT_GREEN, TFT_BLACK );
        Buzzer( 2000, 2000 );

        StartFan ( true );
      }
      else // otherwise YELL at the user to open the oven door
      {
        if ( buzzerCount > 0 )
        {
          DrawHeading( "OPEN OVEN", TFT_RED, TFT_BLACK );
          Buzzer( 2000, 2000 );
          buzzerCount--;
        }
      }
    }
  }
  else
  {
    // YELL at the user to open the oven door
    if ( !isCuttoff && set.useFan )
    {
      DrawHeading( "OPEN OVEN", TFT_GREEN, TFT_BLACK );
      Buzzer( 2000, 2000 );

      debug_print( "CUTOFF: " );
    }
    else if ( !set.useFan )
    {
      StartFan ( false );
    }

    isCuttoff = true;
  }

  currentDetla = (wantedTemp - currentTemp);

  debug_print( "  Delta: " );
  debug_print( currentDetla );

  float base = 128;

  if ( currentDetla >= 0 )
  {
    base = 128 + ( currentDetla * 5 );
  }
  else if ( currentDetla < 0 )
  {
    base = 32 + ( currentDetla * 15 );
  }

  base = constrain( base, 0, 255 );

  debug_print("  Base: ");
  debug_print( base );
  debug_print( " -> " );

  duty = base + ( 172 * perc );
  duty = constrain( duty, 0, 255 );

  // override for full blast at start only if the current Temp is less than the wanted Temp, and it's in the ram before pre-soak starts.
  if ( set.startFullBlast && timeX < solderPaste[currentGraphIndex].reflowTime[1] && currentTemp < wantedTemp )
    duty = 255;

  currentPlotColor = TFT_GREEN;

  SetRelayFrequency( duty );
}

void StartFan ( bool start )
{
  start = !start;
  if ( set.useFan )
  {
    bool isOn = digitalRead( FAN );
    if ( start != isOn )
    {
      debug_print("* Use FAN? ");
      debug_print( set.useFan );
      debug_print( " Should Start? ");
      debug_println( start );

      digitalWrite ( FAN, ( start ? HIGH : LOW ) );
    }
    isFanOn = start;
  }
  else
  {
    isFanOn = false;
  }
}

void DrawHeading( String lbl, unsigned int acolor, unsigned int bcolor )
{
  tft.setTextSize(2); //2
  tft.setTextColor(acolor , bcolor);
  tft.setCursor(0, 0);
  tft.fillRect(0, 0, 160, 20, TFT_BLACK );
  tft.println( String(lbl) );
}

// buzz the buzzer
void Buzzer( int hertz, int len )
{
  // Exit early if no beep is set in settings
  if (!set.beep )
    return;

  tone( BUZZER, hertz, len);
}

// Startup Tune
void BuzzerStart()
{
  tone( BUZZER, 262, 200);
  delay(210);
  tone( BUZZER, 523, 100);
  delay(150);
  tone( BUZZER, 523, 100);
  delay(150);
}


double ox , oy ;

void DrawBaseGraph()
{
  oy = 118;
  ox = 20;
  timeX = 0;

  for ( int ii = 0; ii <= graphRangeMax_X; ii += 5 )
  {
    GraphDefault(ii, solderPaste[currentGraphIndex].wantedCurve[ii], 20, 118, 140, 98, TFT_PINK );
  }

  ox = 20;
  oy = 118;
  timeX = 0;
}

void BootScreen()
{
  tft.fillScreen(TFT_BLACK);
  //tft.drawBitmap( 115, ( tft.height() / 2 ) +20 , UM_Logo, 90, 49, TFT_WHITE);
  state = MENU;
}

void ShowMenu()
{
  state = MENU;

  cachedCurrentTemp = 0;
  SetRelayFrequency( 0 );

  set = NVS_store_read();

  tft.fillScreen(TFT_BLACK);

  tft.setTextColor( TFT_WHITE, TFT_BLACK );
  tft.setTextSize(1);
  tft.setCursor( 10, 10 );
  tft.println( "CURRENT PASTE" );

  debug_print("Cur Graph: ");
  debug_println(currentGraphIndex);
  
  tft.setTextColor( TFT_YELLOW, TFT_BLACK );
  tft.setCursor( 10, 20 );
  tft.println( solderPaste[currentGraphIndex].n );
  tft.setCursor( 10, 30 );
  tft.println( String(solderPaste[currentGraphIndex].tempDeg) + "deg");

  if ( newSettings )
  {
    tft.setTextColor( TFT_CYAN, TFT_BLACK );
    tft.setCursor( 10, tft.height() - 20 );
    tft.println("Settings");
    tft.setCursor( 10, tft.height() - 10 );
    tft.println("Stomped!!");
  }

  ShowMenuOptions();
}

void ShowSettings()
{
  state = SETTINGS;
  SetRelayFrequency( 0 );

  newSettings = false;

  int posY = 20;
  int incY = 10;

  tft.setTextColor( TFT_BLUE, TFT_BLACK );
  tft.fillScreen(TFT_BLACK);

  tft.setTextColor( TFT_BLUE, TFT_BLACK );
  tft.setTextSize(2);
  tft.setCursor( 10, 0 );
  tft.println( "SETTINGS" );

  tft.setTextSize(1);
  tft.setTextColor( TFT_WHITE, TFT_BLACK );
  tft.setCursor( 10, posY );
  tft.print( "SWITCH PASTE" );

  posY += incY;

  // y 64
  UpdateSettingsFan( posY );

  posY += incY;

  // y 83
  UpdateSettingsFanTime( posY );

  posY += incY;

  // y 102
  UpdateSettingsLookAhead( posY );

  posY += incY;

  // y 121
  UpdateSettingsPower( posY );

  posY += incY;

  // y 140
  UpdateSettingsTempOffset( posY );

  posY += incY;

  // y 159
  UpdateSettingsStartFullBlast( posY );

  posY += incY;

  // y 178
  UpdateSettingsBakeTempGap( posY );

  posY += incY;
  tft.setTextColor( TFT_WHITE, TFT_BLACK );
  tft.setCursor( 10, posY );
  tft.print( "RESET TO DEFAULTS" );

  posY += incY;

  ShowMenuOptions();
}

void ShowPaste()
{
  state = SETTINGS_PASTE;
  SetRelayFrequency( 0 );

  tft.fillScreen(TFT_BLACK);

  tft.setTextColor( TFT_BLUE, TFT_BLACK );
  tft.setTextSize(2);
  tft.setCursor( 10, 10 );
  tft.println( "SWITCH PASTE" );

  tft.setTextColor( TFT_WHITE, TFT_BLACK );

  int y = 30;

  for ( size_t i = 0; i < ELEMENTS(solderPaste); i++ )
  {
    if ( i == set.paste )
      tft.setTextColor( TFT_YELLOW, TFT_BLACK );
    else
      tft.setTextColor( TFT_WHITE, TFT_BLACK );

    tft.setTextSize(1);
    tft.setCursor( 10, y );

    tft.println( String( solderPaste[i].tempDeg) + "d " + solderPaste[i].n );
    tft.setTextSize(1);
    tft.setCursor( 10, y + 10 );
    tft.println( solderPaste[i].t );
    tft.setTextColor( TFT_LIGHTGREY, TFT_BLACK );

    y += 20;
  }

  ShowMenuOptions();
}

void ShowMenuOptions()
{
  if ( state == SETTINGS_PASTE || state == SETTINGS)
  {
    UpdateSettingsPointer();
  }
  if(state == MENU) {
    tft.setTextSize(1);
    tft.setTextColor( TFT_WHITE, TFT_BLACK );
    tft.setCursor( 10, tft.height() - 45);
    tft.println( "CURRENT MODE: " );
    tft.setTextColor( TFT_YELLOW, TFT_BLACK );
    tft.setCursor( 10, tft.height() - 35);
    switch(galet) {
      case REHEAT:  tft.println( "REFLOW" );  break;
      case BAKES:   tft.println( "BAKE  " );  break;
      case COOKIES: tft.println( "COOKIE" );  break;
      case ROST:    tft.println( "ROST  " );  break;
      case GRILL:   tft.println( "GRILL " );  break;
      case TOAST:   tft.println( "TOAST " );  break;
      case CRUMPET: tft.println( "CRUMPT" );  break;
      case PIZZA:   tft.println( "PIZZA " );  break;
    }
  }
}

void UpdateBakeMenu()
{
  tft.setTextColor( TFT_BLUE, TFT_BLACK );
  tft.setTextSize(1);
  tft.fillRect( 0, 10, 10, tft.height() - 10, TFT_BLACK );
  tft.setCursor( 0, ( 55 + ( 40 * enc_mode ) ) );
  tft.println(">");

  tft.setTextColor( TFT_YELLOW, TFT_BLACK );
  tft.setTextSize(3); //2
  tft.setCursor( 10, 50 );
  tft.println(String((round(set.bakeTemp)),0) + "c ");
  tft.setCursor( 10, 90 );
  tft.println(String(set.bakeTime / 60) + "min ");
}

void ShowBakeMenu()
{
  state = BAKE_MENU;

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor( TFT_BLUE, TFT_BLACK );
  tft.setTextSize(2); //2
  tft.setCursor( 10, 10 );
  tft.println( "LET'S BAKE!" );

  tft.setTextColor( TFT_WHITE, TFT_BLACK );
  tft.setCursor( 10, 40 );
  tft.setTextSize(1);
  tft.println("TEMPERATURE");
  tft.setCursor( 10, 80 );
  tft.println( "TIME");

  tft.setTextColor( TFT_WHITE, TFT_BLACK );
  tft.setTextSize(1);


  UpdateBakeMenu();
}

void UpdateBake()
{
  tft.setTextColor( TFT_BLUE, TFT_BLACK );
  tft.setTextSize(2); //2
  tft.setCursor( 10, 10 );

  switch (currentBakeTimeCounter)
  {
    case 0:
      tft.println( "BAKING   " );
      break;
    case 3:
      tft.println( "BAKING.  " );
      break;
    case 6:
      tft.println( "BAKING.. " );
      break;
    case 9:
      tft.println( "BAKING..." );
      break;
    default:
      // do nothing
      break;
  }

  currentBakeTimeCounter++;
  if (currentBakeTimeCounter == 12)
    currentBakeTimeCounter = 0;

  tft.setTextColor( TFT_YELLOW, TFT_BLACK );
  tft.setTextSize(3); //2
  tft.setCursor( 10, 50 );
  tft.println(String((round(currentTemp * 10) / 10), 0) + "/" + String(round(set.bakeTemp), 0) + "c");
  tft.setCursor( 10, 90 );
  tft.println(String((round(currentBakeTime / 60 + 0.5)),0) + "/" + String((set.bakeTime / 60)) + "min ");

  if ( round( currentTemp ) > round( cachedCurrentTemp ) )
    lastTempDirection = 1;
  else if ( round( currentTemp ) < round( cachedCurrentTemp ) )
    lastTempDirection = -1;

  cachedCurrentTemp = currentTemp;
}

void StartBake()
{
  currentBakeTime = set.bakeTime;
  currentBakeTimeCounter = 0;

  tft.fillScreen(TFT_BLACK);

  //  tft.setTextColor( TFT_BLUE, TFT_BLACK );
  //  tft.setTextSize(1); //2
  //  tft.setCursor( 10, 20 );
  //  tft.println( "BAKING..." );

  tft.setTextColor( TFT_WHITE, TFT_BLACK );
  tft.setCursor( 10, 40 );
  tft.setTextSize(1);
  tft.println("CURRENT TEMP");
  tft.setCursor( 10, 80 );
  tft.println( "TIME LEFT");

  UpdateBake();
}

void BakeDone()
{
  state = BAKE_DONE;
  
  SetRelayFrequency(0); // Turn the SSR off immediately

  Buzzer( 2000, 500 );

  if ( set.useFan && set.fanTimeAfterReflow > 0 )
  {
    keepFanOnTime = millis() + set.fanTimeAfterReflow * 1000;
    StartFan( true );
  }
  else
  {
    StartFan( false );
  }

  tft.fillScreen(TFT_BLACK);

  tft.setTextColor( TFT_BLUE, TFT_BLACK );
  tft.setTextSize(2); //2
  println_Center("BAKING DONE!", tft.width() / 2, ( tft.height() / 2 ) );

  tft.setTextColor( TFT_WHITE, TFT_BLACK );
  tft.setTextSize(1);

  delay(750);
  Buzzer( 2000, 500 );

}

void UpdateSettingsPointer()
{
  if ( state == SETTINGS )
  {
    tft.setTextColor( TFT_BLUE, TFT_BLACK );
    tft.setTextSize(1);
    tft.fillRect( 0, 10, 10, tft.height() - 10, TFT_BLACK );
    tft.setCursor( 0, ( 20 + ( 10 * settings_pointer ) ) );
    tft.println(">");

    tft.setTextSize(1);
    tft.setTextColor( TFT_GREEN, TFT_BLACK );
    tft.fillRect( 0, tft.height() - 20, tft.width(), 20, TFT_BLACK );

    tft.setCursor(0, tft.height() - 20);
    switch ( settings_pointer )
    {
      case 0:
        tft.println("Select which profile to reflow");
        break;

      case 1:
        tft.println("Enable fan for end of reflow, requires 5V DC fan");
        break;

      case 2:
        tft.println("Keep fan on for XXX sec after reflow");
        break;

      case 3:
        tft.println("Soak and Reflow look ahead for rate change speed");
        break;

      case 4:
        tft.println("Adjust the power boost");
        break;

      case 5:
        tft.println("Adjust temp probe reading offset");
        break;

      case 6:
        tft.println("Force full power on initial ramp-up - be careful!");
        break;

      case 7:
        tft.println("Bake thermal mass adjustment, higher for more mass");
        break;

      case 8:
        tft.println("Reset to default settings");
        break;

      default:
        //tft.println("", tft.width() / 2, tft.height() - 20 );
        break;
    }
    tft.setTextSize(1);
  }
  else if ( state == SETTINGS_PASTE )
  {
    tft.setTextColor( TFT_BLUE, TFT_BLACK );
    tft.setTextSize(1);
    tft.fillRect( 0, 20, 10, tft.height(), TFT_BLACK );
    tft.setCursor( 5, ( 30 + ( 10 * ( settings_pointer * 2 ) ) ) );
    tft.println(">");
  }
}

void StartWarmup()
{
  tft.fillScreen(TFT_BLACK);

  state = WARMUP;
  timeX = 0;
  ShowMenuOptions();
  lastWantedTemp = -1;
  buzzerCount = 5;
  keepFanOnTime = 0;
  StartFan( false );

  tft.setTextColor( TFT_BLUE, TFT_BLACK );
  tft.setTextSize(2); //2
  tft.setCursor( 10, 10 );
  tft.println( "REFLOW..." );

  tft.setTextColor( TFT_GREEN, TFT_BLACK );
  tft.setTextSize(2); //2
  println_Center("WARMING UP", tft.width() / 2, ( (tft.height() / 2) - 20 ) );
  tft.setTextSize(1);
  tft.setTextColor( TFT_WHITE, TFT_BLACK );
  println_Center("START @ " + String( GetGraphValue(0)) + "c", tft.width() / 2, ( tft.height() / 2 ) + 30 );
}

void StartReflow()
{
  tft.fillScreen(TFT_BLACK);

  state = REFLOW;
  ShowMenuOptions();

  timeX = 0;
  SetupGraph(0, 0, 20, 118, 140, 98, graphRangeMin_X, graphRangeMax_X, graphRangeStep_X, graphRangeMin_Y, graphRangeMax_Y, graphRangeStep_Y, "Reflow Temp", " Time [s]", "deg [C]", TFT_DARKCYAN, TFT_BLUE, TFT_WHITE, TFT_BLACK );

  DrawHeading( "READY", TFT_WHITE, TFT_BLACK );
  DrawBaseGraph();
}

void AbortReflow()
{

  if ( state == WARMUP || state == REFLOW || state == OVENCHECK_START || state == BAKE ) // if we are in warmup or reflow states
  {
    state = ABORT;

    SetRelayFrequency(0); // Turn the SSR off immediately

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor( TFT_RED, TFT_BLACK );
    tft.setTextSize(3); //2
    println_Center("ABORT", tft.width() / 2, ( tft.height() / 2 ) );

    if ( set.useFan && set.fanTimeAfterReflow > 0 )
    {
      keepFanOnTime = millis() + set.fanTimeAfterReflow * 1000;
    }
    else
    {
      StartFan( false );
    }

    delay(1000);
    
    state = MENU;
    ShowMenu();
  }
}

void EndReflow()
{
  if ( state == REFLOW )
  {
    SetRelayFrequency( 0 );
    state = FINISHED;

    Buzzer( 2000, 500 );

    DrawHeading( "DONE!", TFT_WHITE, TFT_BLACK );

    ShowMenuOptions();

    if ( set.useFan && set.fanTimeAfterReflow > 0 )
    {
      keepFanOnTime = millis() + set.fanTimeAfterReflow * 1000;
    }
    else
    {
      StartFan( false );
    }

    delay(750);
    Buzzer( 2000, 500 );
  }
}

void SetDefaults()
{
  // Default settings values
  set.valid = true;
  set.fanTimeAfterReflow = 60;
  set.power = 1;
  set.paste = 0;
  set.useFan = false;
  set.lookAhead = 7;
  set.lookAheadWarm = 7;
  set.startFullBlast = false;
  set.tempOffset = 0;
  set.beep = true;
  set.bakeTime = 1200;
  set.bakeTemp = 45;
  set.bakeTempGap = 3;
}

void ResetSettingsToDefault()
{
  // set default values again and save
  SetDefaults();
  NVS_store_write(set);

  // load the default paste
  SetCurrentGraph( set.paste );

  // show settings
  settings_pointer = 0;
  ShowSettings();
}


/*
   OVEN CHECK CODE NOT CURRENTLY USED
   START
*/
void StartOvenCheck()
{
  debug_println("Oven Check Start Temp " + String( currentTemp ) );

  state = OVENCHECK_START;
  calibrationSeconds = 0;
  calibrationState = 0;
  calibrationStatsUp = 0;
  calibrationStatsDown = 300;
  calibrationUpMatch = false;
  calibrationDownMatch = false;
  calibrationDropVal = 0;
  calibrationRiseVal = 0;
  SetRelayFrequency( 0 );
  StartFan( false );

  debug_println("Running Oven Check");

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor( TFT_CYAN, TFT_BLACK );
  tft.setTextSize(2);
  tft.setCursor( 10, 10 );
  tft.println( "OVEN CHECK" );

  tft.setTextSize(1);
  tft.setTextColor( TFT_YELLOW, TFT_BLACK );
  tft.setCursor( 10, 30 );
  tft.println( solderPaste[currentGraphIndex].n );
  tft.setCursor( 10, 40 );
  tft.println( String(solderPaste[currentGraphIndex].tempDeg) + "deg");

  ShowMenuOptions();
}

void ShowOvenCheck()
{
  state = OVENCHECK;
  SetRelayFrequency( 0 );
  StartFan( true );

  debug_println("Oven Check");

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor( TFT_CYAN, TFT_BLACK );
  tft.setTextSize(2);
  tft.setCursor( 10, 10 );
  tft.println( "OVEN CHECK" );


  tft.setTextColor( TFT_WHITE, TFT_BLACK );

  ShowMenuOptions();

  tft.setTextSize(1);
  tft.setCursor( 0, 30 );
  tft.setTextColor( TFT_YELLOW, TFT_BLACK );

  tft.println( " Oven Check allows");
  tft.println( " you to measure your");
  tft.println( " oven's rate of heat");
  tft.println( " up & cool down to");
  tft.println( " see if your oven");
  tft.println( " is powerful enough");
  tft.println( " to reflow your ");
  tft.println( " selected profile.");
  tft.println( "");

  tft.setTextColor( TFT_YELLOW, TFT_RED );
  tft.println( " EMPTY YOUR OVEN FIRST!");
}

/*
   END
   OVEN CHECK CODE NOT CURRENTLY USED/WORKING
*/


/*
   Lots of UI code here
*/

void ShowResetDefaults()
{
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor( TFT_WHITE, TFT_BLACK );
  tft.setTextSize(2);
  tft.setCursor( 10, 50 );

  tft.setTextColor( TFT_WHITE, TFT_BLACK );
  tft.print( "RESET SETTINGS" );
  tft.setCursor( 10, 70 );
  tft.println( "ARE YOU SURE?" );

  state = SETTINGS_RESET;
  ShowMenuOptions();

  tft.setTextSize(1);
  tft.setTextColor( TFT_GREEN, TFT_BLACK );
  tft.fillRect( 0, tft.height() - 40, tft.width(), 40, TFT_BLACK );
  tft.setCursor( 10, 100 );
  tft.println("Settings restore cannot be undone!");
}

void UpdateSettingsFan( int posY )
{
  tft.fillRect( 0,  posY, 160, 10, TFT_BLACK );
  tft.setTextColor( TFT_WHITE, TFT_BLACK );
  tft.setCursor( 10, posY );

  tft.setTextColor( TFT_WHITE, TFT_BLACK );
  tft.print( "USE FAN " );
  tft.setTextColor( TFT_YELLOW, TFT_BLACK );

  if ( set.useFan )
  {
    tft.println( "ON" );
  }
  else
  {
    tft.println( "OFF" );
  }
  tft.setTextColor( TFT_WHITE, TFT_BLACK );
}

void UpdateSettingsFanTime( int posY )
{
  tft.fillRect( 0,  posY, 160, 10, TFT_BLACK );
  tft.setTextColor( TFT_WHITE, TFT_BLACK );
  tft.setCursor( 10, posY );

  tft.setTextColor( TFT_WHITE, TFT_BLACK );
  tft.print( "FAN COUNTDOWN " );
  tft.setTextColor( TFT_YELLOW, TFT_BLACK );

  tft.println( String( set.fanTimeAfterReflow ) + "s");

  tft.setTextColor( TFT_WHITE, TFT_BLACK );
}

void UpdateSettingsStartFullBlast( int posY )
{
  tft.fillRect( 0,  posY, 160, 10, TFT_BLACK );
  tft.setTextColor( TFT_WHITE, TFT_BLACK );
  tft.setCursor( 10, posY );
  tft.print( "START RAMP 100% " );
  tft.setTextColor( TFT_YELLOW, TFT_BLACK );

  if ( set.startFullBlast )
  {
    tft.println( "ON" );
  }
  else
  {
    tft.println( "OFF" );
  }
  tft.setTextColor( TFT_WHITE, TFT_BLACK );
}

void UpdateSettingsPower( int posY )
{
  tft.fillRect( 0,  posY, 160, 10, TFT_BLACK );
  tft.setTextColor( TFT_WHITE, TFT_BLACK );

  tft.setCursor( 10, posY );
  tft.print( "POWER ");
  tft.setTextColor( TFT_YELLOW, TFT_BLACK );
  tft.println( String( round((set.power * 100))) + "%");
  tft.setTextColor( TFT_WHITE, TFT_BLACK );
}

void UpdateSettingsLookAhead( int posY )
{
  tft.fillRect( 0,  posY, 160, 10, TFT_BLACK );
  tft.setTextColor( TFT_WHITE, TFT_BLACK );

  tft.setCursor( 10, posY );
  tft.print( "GRAPH LOOK AHEAD ");
  tft.setTextColor( TFT_YELLOW, TFT_BLACK );
  tft.println( String( set.lookAhead) );
  tft.setTextColor( TFT_WHITE, TFT_BLACK );
}

void UpdateSettingsBakeTempGap( int posY )
{
  tft.fillRect( 0,  posY, 160, 10, TFT_BLACK );
  tft.setTextColor( TFT_WHITE, TFT_BLACK );

  tft.setCursor( 10, posY );
  tft.print( "BAKE TEMP GAP ");
  tft.setTextColor( TFT_YELLOW, TFT_BLACK );
  tft.println( String( set.bakeTempGap ) );
  tft.setTextColor( TFT_WHITE, TFT_BLACK );
}

void UpdateSettingsTempOffset( int posY )
{
  tft.fillRect( 0,  posY, 160, 10, TFT_BLACK );
  tft.setTextColor( TFT_WHITE, TFT_BLACK );

  tft.setCursor( 10, posY );
  tft.print( "TEMP OFFSET ");
  tft.setTextColor( TFT_YELLOW, TFT_BLACK );
  tft.println( String( set.tempOffset) );
  tft.setTextColor( TFT_WHITE, TFT_BLACK );
}

/*
   Button press code here
*/

unsigned long nextButtonPress = 0;

void button0Press()   //BUT_TEMP
{
  if ( nextButtonPress < millis() )
  {
    nextButtonPress = millis() + 150;
    Buzzer( 2000, 50 );

    if ( state == BAKE_MENU )
    {
      if(enc_mode == BAKE_MENU_TEMP) enc_mode = BAKE_MENU_TIME;
      else if(enc_mode == BAKE_MENU_TIME) enc_mode = BAKE_MENU_TEMP;
      UpdateBakeMenu();
    }
    else if ( state == FINISHED || state == BAKE_DONE )
    {
      
      ShowMenu();
    }
    else if ( state == SETTINGS )
    {
      if ( settings_pointer == 0 )  // change paste
      {
        settings_pointer = set.paste;
        ShowPaste();
      }
      else if ( settings_pointer == 1 )  // switch fan use
      {
        set.useFan = !set.useFan;

        UpdateSettingsFan( 30 );
      }
      else if ( settings_pointer == 2 ) // fan countdown after reflow
      {
        set.fanTimeAfterReflow += 5;
        if ( set.fanTimeAfterReflow > 60 )
          set.fanTimeAfterReflow = 0;

        UpdateSettingsFanTime( 40 );
      }
      else if ( settings_pointer == 3 ) // change lookahead for reflow
      {
        set.lookAhead += 1;
        if ( set.lookAhead > 15 )
          set.lookAhead = 1;

        UpdateSettingsLookAhead( 50 );
      }
      else if ( settings_pointer == 4 ) // change power
      {
        set.power += 0.1;
        if ( set.power > 1.55 )
          set.power = 0.5;

        UpdateSettingsPower( 60 );
      }
      else if ( settings_pointer == 5 ) // change temp probe offset
      {
        set.tempOffset += 1;
        if ( set.tempOffset > 15 )
          set.tempOffset = -15;

        UpdateSettingsTempOffset( 70 );
      }
      else if ( settings_pointer == 6 ) // change use full power on initial ramp
      {
        set.startFullBlast = !set.startFullBlast;

        UpdateSettingsStartFullBlast( 80 );
      }
      else if ( settings_pointer == 7 ) // bake temp gap
      {
        set.bakeTempGap += 1;
        if ( set.bakeTempGap > 5 )
          set.bakeTempGap = 0;

        UpdateSettingsBakeTempGap( 90 );
      }
      else if ( settings_pointer == 8 ) // reset defaults
      {
        ShowResetDefaults();
      }
      ShowMenuOptions();
    }
    else if ( state == SETTINGS_PASTE )
    {
      if ( set.paste != settings_pointer )
      {
        set.paste = settings_pointer;
        debug_print("Paste set:");
        debug_println(set.paste);
        SetCurrentGraph( set.paste );
        debug_println("Paste SET - OK");
        ShowPaste();
      }
    }
    else if ( state == SETTINGS_RESET )
    {
      ResetSettingsToDefault();
    }
    else if ( state == OVENCHECK )
    {
      StartOvenCheck();
    }
  }
}

void button1Press()   //BUT_START
{
  if ( nextButtonPress < millis() )
  {
    nextButtonPress = millis() + 150;
    Buzzer( 2000, 50 );

    if ( state == MENU )
    {
      switch(galet) {
        case REHEAT:
          // Only allow reflow start if there is no TC error
          if ( tcError == 0 ) StartWarmup();
          else                Buzzer( 100, 250 );
        break;

        case BAKES:
          // Only allow reflow start if there is no TC error
          if ( tcError == 0 ) ShowBakeMenu();
          else                Buzzer( 100, 250 );
        break;

      }
    }
    else if ( state == BAKE_MENU )
    {
      state = BAKE;
      StartBake();
    }
  }
}

void button2Press()   //BUT_COOLING
{
  if ( nextButtonPress < millis() )
  {
    nextButtonPress = millis() + 150;
    Buzzer( 2000, 50 );

    if ( state == MENU )
    {
      settings_pointer = 0;
      ShowSettings();
    }
    else if ( state == WARMUP || state == REFLOW || state == OVENCHECK_START || state == BAKE )
    {
      AbortReflow();
    }
    else if ( state == SETTINGS ) // leaving settings so save
    {
      // save data in flash
      NVS_store_write(set);
      
      ShowMenu();
    }
    else if ( state == SETTINGS_PASTE || state == SETTINGS_RESET )
    {
      settings_pointer = 0;
      ShowSettings();
    }
    else if ( state == OVENCHECK ) // cancel oven check
    {
      ShowMenu();
    }
    else if ( state == BAKE_MENU) // cancel oven check
    {
      NVS_store_write(set);
      ShowMenu();
    }
    
  }
}


void button3Press()   //BUT_CF
{
  if ( nextButtonPress < millis() )
  {
    nextButtonPress = millis() + 150;
    Buzzer( 2000, 50 );

    if ( state == MENU )
    {
      // Only allow reflow start if there is no TC error
      if ( tcError == 0 )
        ShowOvenCheck();
      else
        Buzzer( 100, 250 );
    }
  }
}

void encRight() {
    Buzzer( 2000, 50 );
    if ( state == BAKE_MENU && enc_mode == BAKE_MENU_TEMP)
    {
      set.bakeTemp += 1;
      if ( set.bakeTemp > maxBakeTemp )
        set.bakeTemp = minBakeTemp;

      UpdateBakeMenu();
    }
    else if ( state == BAKE_MENU && enc_mode == BAKE_MENU_TIME)
    {
      set.bakeTime += 300;
      if ( set.bakeTime > maxBakeTime )
        set.bakeTime = maxBakeTime;

      UpdateBakeMenu();
    }
    else if ( state == SETTINGS )
    {
      settings_pointer = constrain( settings_pointer + 1, 0, 8 );
      ShowMenuOptions();
      //UpdateSettingsPointer();
    }
    else if ( state == SETTINGS_PASTE )
    {
      settings_pointer = constrain( settings_pointer + 1, 0, (int) ELEMENTS(solderPaste) - 1 );
      UpdateSettingsPointer();
    }

}


void encLeft() {
  Buzzer( 2000, 50 );
  if ( state == BAKE_MENU && enc_mode == BAKE_MENU_TEMP)
  {
    set.bakeTemp -= 1;
    if ( set.bakeTemp < minBakeTemp )
      set.bakeTemp = minBakeTemp;

    UpdateBakeMenu();
  }
  else if ( state == BAKE_MENU && enc_mode == BAKE_MENU_TIME)
  {
    set.bakeTime -= 300;
    if ( set.bakeTime < minBakeTime )
      set.bakeTime = minBakeTime;

    UpdateBakeMenu();
  }
  else if ( state == SETTINGS )
  {
    settings_pointer = constrain( settings_pointer - 1, 0, 8 );
    ShowMenuOptions();
    //UpdateSettingsPointer();
  }
  else if ( state == SETTINGS_PASTE )
  {
    settings_pointer = constrain( settings_pointer - 1, 0, (int) ELEMENTS(solderPaste) - 1 );
    UpdateSettingsPointer();
  }

}

void buttonTask(void *pvParameters ) { 
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
    else if(adc_galet < 1800  && adc_galet >= 1300)    galet = BAKES;
    else if(adc_galet < 1300   && adc_galet >= 800)    galet = TOAST;
    else if(adc_galet < 800   && adc_galet >= 300)     galet = CRUMPET;
    else if(adc_galet < 300)                           galet = PIZZA;

    if(button == BUT_START)   button1Press(); 
    if(button == BUT_TEMP)    button0Press();
    if(button == BUT_COOLING) button2Press();
    if(button == BUT_CF)      button3Press();
    
    if (enc.isRight()) encRight();
    if (enc.isLeft()) encLeft();

    if(galet_old != galet) {
      galet_old = galet;
      ShowMenuOptions();
    }
    vTaskDelay(10);
  }
  vTaskDelete(NULL);
}

/*
   Graph drawing code here
   Special thanks to Kris Kasprzak for his free graphing code that I derived mine from
   https://www.youtube.com/watch?v=YejRbIKe6e0
*/

void SetupGraph(double x, double y, double gx, double gy, double w, double h, double xlo, double xhi, double xinc, double ylo, double yhi, double yinc, String title, String xlabel, String ylabel, unsigned int gcolor, unsigned int acolor, unsigned int tcolor, unsigned int bcolor )
{
  double i;
  int temp;

  ox = (x - xlo) * ( w) / (xhi - xlo) + gx;
  oy = (y - ylo) * (gy - h - gy) / (yhi - ylo) + gy;
  // draw y scale
  for ( i = ylo; i <= yhi; i += yinc)
  {
    // compute the transform
    temp =  (i - ylo) * (gy - h - gy) / (yhi - ylo) + gy;

    if (i == ylo) {
      tft.drawLine(gx, temp, gx + w, temp, acolor);
    }
    else {
      tft.drawLine(gx, temp, gx + w, temp, gcolor);
    }

    tft.setTextSize(1);
    tft.setTextColor(tcolor, bcolor);
    tft.setCursor(gx - 20, temp);
    println_Right(String(round(i), 0), gx - 20, temp );
  }

  // draw x scale
  for (i = xlo; i <= xhi; i += xinc)
  {
    temp =  (i - xlo) * ( w) / (xhi - xlo) + gx;
    if (i == 0)
    {
      tft.drawLine(temp, gy, temp, gy - h, acolor);
    }
    else
    {
      tft.drawLine(temp, gy, temp, gy - h, gcolor);
    }

    tft.setTextSize(1);
    tft.setTextColor(tcolor, bcolor);
    tft.setCursor(temp, gy );

    if ( i <= xhi - xinc )
      println_Center(String(round(i), 0), temp, gy + 6 );
    else
      println_Center(String(round(xhi), 0), temp, gy + 6);
  }

  //now draw the labels
  tft.setTextSize(1);
  tft.setTextColor(tcolor, bcolor);
  tft.setCursor(gx , gy - h - 10);
  tft.println(title);

  tft.setTextSize(1);
  tft.setTextColor(acolor, bcolor);
  tft.setCursor(w - 40 , gy - 10);
  tft.println(xlabel);

  tft.setRotation(2);
  tft.setTextSize(1);
  tft.setTextColor(acolor, bcolor);
  tft.setCursor(h - 35, 22 );
  tft.println(ylabel);
  tft.setRotation(3);
}


void Graph(double x, double y, double gx, double gy, double w, double h )
{
  // recall that ox and oy are initialized as static above
  x =  (x - graphRangeMin_X) * ( w) / (graphRangeMax_X - graphRangeMin_X) + gx;
  y =  (y - graphRangeMin_Y) * (gy - h - gy) / (graphRangeMax_Y - graphRangeMin_Y) + gy;

  if ( timeX < 2 )
    oy = min( oy, y );

  y = min( (int)y, 118 ); // bottom of graph!

  //  tft.fillRect( ox-1, oy-1, 3, 3, currentPlotColor );

  tft.drawLine(ox, oy + 1, x, y + 1, currentPlotColor);
  tft.drawLine(ox, oy - 1, x, y - 1, currentPlotColor);
  tft.drawLine(ox, oy, x, y, currentPlotColor );
  ox = x;
  oy = y;
}

void GraphDefault(double x, double y, double gx, double gy, double w, double h, unsigned int pcolor )
{
  // recall that ox and oy are initialized as static above
  x =  (x - graphRangeMin_X) * ( w) / (graphRangeMax_X - graphRangeMin_X) + gx;
  y =  (y - graphRangeMin_Y) * (gy - h - gy) / (graphRangeMax_Y - graphRangeMin_Y) + gy;

  //Serial.println( oy );
  tft.drawLine(ox, oy, x, y, pcolor);
  tft.drawLine(ox, oy + 1, x, y + 1, pcolor);
  tft.drawLine(ox, oy - 1, x, y - 1, pcolor);
  ox = x;
  oy = y;
}


void println_Center(String heading, int centerX, int centerY )
{
  int x = 0;
  int y = 0;

  uint8_t ww = tft.textWidth(heading.c_str());
  uint8_t hh = tft.fontHeight();
  //tft.getTextBounds( heading.c_str(), x, y, &x1, &y1, &ww, &hh );
  tft.setCursor( centerX - ww / 2 + 2, centerY - hh / 2);
  tft.println( heading );
}

void println_Right(String heading, int centerX, int centerY )
{
  int x = 0;
  int y = 0;

  uint8_t ww = tft.textWidth(heading.c_str());
  uint8_t hh = tft.fontHeight();

  tft.setCursor( centerX + ( 18 - ww ), centerY - hh / 2);
  tft.println( heading );
}


