#include <Arduino.h>
#include <TFT_eSPI.h> 
#include <SPI.h>
#include <MAX31855soft.h>
#include <GyverEncoder.h>
#include <ArduinoNVS.h>
#include "spline.h"
#include "ReflowMasterProfile.h"


void buttonTask(void *pvParameters);
void SetupGraph(double x, double y, double gx, double gy, double w, double h, double xlo, double xhi, double xinc, double ylo, double yhi, double yinc, String title, String xlabel, String ylabel, unsigned int gcolor, unsigned int acolor, unsigned int tcolor, unsigned int bcolor );
void Graph(double x, double y, double gx, double gy, double w, double h );
void GraphDefault(double x, double y, double gx, double gy, double w, double h, unsigned int pcolor );
void println_Center(String heading, int centerX, int centerY );
void println_Right(String heading, int centerX, int centerY );
void LoadPaste();
int GetGraphValue( int x );
int GetGraphTime( int x );
void SetCurrentGraph( int index );
void DisplayTemp( bool center = false );
void SetRelayFrequency( int duty );
void MatchCalibrationTemp();
void KeepFanOnCheck();
void ReadCurrentTempAvg();
void ReadCurrentTemp();
void MatchTemp_Bake();
void MatchTemp();
void StartFan ( bool start );
void DrawHeading( String lbl, unsigned int acolor, unsigned int bcolor );
void Buzzer( int hertz, int len );
void BuzzerStart();
void DrawBaseGraph();
void BootScreen();
void ShowMenu();
void ShowSettings();
void ShowPaste();
void ShowMenuOptions();
void UpdateBakeMenu();
void ShowBakeMenu();
void UpdateBake();
void StartBake();
void BakeDone();
void UpdateSettingsPointer();
void StartWarmup();
void StartReflow();
void AbortReflow();
void EndReflow();
void SetDefaults();
void ResetSettingsToDefault();
void StartOvenCheck();
void ShowOvenCheck();
void ShowResetDefaults();
void UpdateSettingsFan( int posY );
void UpdateSettingsFanTime( int posY );
void UpdateSettingsStartFullBlast( int posY );
void UpdateSettingsPower( int posY );
void UpdateSettingsLookAhead( int posY );
void UpdateSettingsBakeTempGap( int posY );
void UpdateSettingsTempOffset( int posY );
