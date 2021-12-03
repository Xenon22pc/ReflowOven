#include <Arduino.h>
#include <TFT_eSPI.h> 
#include <SPI.h>
#include <MAX31855soft.h>
#include "spline.h"
#include "ReflowMasterProfile.h"

void LoadPaste();
int GetGraphValue( int x );
int GetGraphTime( int x );
ReflowGraph CurrentGraph();
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
