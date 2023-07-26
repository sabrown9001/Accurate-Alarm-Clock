/******************************************************************************
* Real Time Clock with Alarm
*******************************************************************************
*
* by David Schmidt
* modified by Stephen A. Brown
*
* Reads the DS3231 RTC and prints the Date, Time,
* Temprature(Just because it's there), and has a 1 x per day Alarm.
*
* The Alarm pin has been changed to 47 to coincide with the Inventr.io video
* for Adventure Kit 2, Chapter 4, Acurate Alarm Clock.
*
* With the Serial Monitor Input, if the user inputs a string matching:
*     "YYMMDDHHMMSS" (year, month, day, hour, minute, sec)
* That Date & Time will be set into the RTC.
*   Note: Day of Week has been removed as it is calculated by the Library/RTC.
*
* If the user inputs a string matching:
*     "HHMM" (hour, minute)
* That Time will be set as the new Alarm time.
*
* Day of Week is taken care of by the RTC and associated library.
*
* This uses a 20 x 4 LCD with I2C Interface (What I had available with I2C).
* Top 2 lines of the display to show Date, Time & Temprature.
*
* A 16 Key Membraine Keypad has been added for setting the display
* with keys defined as follows:
*   2 = Up Arrow
*   4 = Left Arrow
*   6 = Right Arrow
*   8 = Down Arrow
*   A = Set Alarm
*   D = Set Date&Time / Enter
*
* The Date & Time may be set by pushing "D" (Enter) to begin.
* "2" Up Arrow Increases the Value of the selected field.
* "8" Down Arrow Decreades the value of the selected field.
* "4" Left & "6" Right Arrows moves between fields
* Enter Sets Current Date & Time.
*
* The Alarm may be set by pushing the "A" (Alarm Set) to begin.
* Keys function as above with "D" (Enter) completing the opperation.
*   Note: Alarm Setting is currently only in 24 Hour format.
*
* Note: Because the storage for the Alarm Hour & Minute is not in the RTC,
* it gets reset to 8:30 with a reset of the system.
*
*******************************************************************************/

#include <Keypad.h>  // Library for 4x4 membrane keypad
#include <Wire.h>
#include <LibPrintf.h>
#include <LiquidCrystal_I2C.h>

// Pins
const int buzzerPin = 47;

//
//  º Symbol for Temprature
//
byte degreeSymbol[8] = {
                          B01000,
                          B10100,
                          B01000,
                          B00000,
                          B00000,
                          B00000,
                          B00000,
                          B00000
                        };

//
// Date and time functions using a DS3231 RTC connected via I2C and Wire lib
//
//  RTC_DS3231 Class Reference
//    https://adafruit.github.io/RTClib/html/class_r_t_c___d_s3231.html
//
#include "RTClib.h"
RTC_DS3231 rtc;

//
//  The following Constants and Variables used for setting the Date and Time.
//  The Constants are used to index into the RTCtime[] array to simplify
//  some coding issues.
//
#define ixMonths      0
#define ixDays        1
#define ixYears       2
#define ixHours       3
#define ixMinutes     4
#define ixSeconds     5
#define ixIsPM        6
#define ixIsH24       7
#define ixAlarmHour   8
#define ixAlarmMinute 9

//
//  This is our own structure used to set RTC date and time.
//  The Buttons defined as "Left" and "Right" adjust the index into RTCtime[]
//  array. Out Of Bounds conditions are handeled in the code.
//
//  Except for RTCtime[aIsH24], Initialization values are hold over from 
//  previous vresions of code.  In the event of RTC Power Failure, 
//  RTC is set to Compile Date & Time.
//
short RTCtime[] = {
                    7,      // Month
                    26,     // Day
                    2023,   // Year
                    10,     // Hour
                    25,     // Minute
                    59,     // Second
                    false,  // Is PM?
                            //    true == PM
                            //    false == AM
                    false,  // Is 24 Hour Clock?
                            //    true == 24 Hour Clock
                            //    false == 12 Hour Clock
                    8,      // Alarm Hour
                    30      // Alarm Minute
                  };

short printHours;           // variable to adjust 12 Hour Clock
                            // from 24 hour clock
short prevSecond;

//
//  Arrays for: Lengths of Months
//              Names of Months
//              Names of Days of Week
//
//  To make things as esoteric as possible, MonthName and daysOfTheWeek are 
//  actualy arrays of pointers to character arrays.  Because of this, using 
//  names of different lengths is not a problem. They are all the same
//  length here due to Display Hardware Limitations.
//
short MonthLen[12] =      {     // 30 days hath September, April, 
                                //  June and November
                            31, // Length of  Jan in days.
                            28, //            Feb - Need Leap Yrar Processing
                            31, //            Mar
                            30, //            Apr
                            31, //            May
                            30, //            Jun
                            31, //            Jul
                            31, //            Aug
                            30, //            Sep
                            31, //            Oct
                            30, //            Nov
                            31  //            Dec
                          };

char *MonthName[12] =     {
                            "Jan", 
                            "Feb", 
                            "Mar", 
                            "Apr", 
                            "May", 
                            "Jun", 
                            "Jul", 
                            "Aug", 
                            "Sep", 
                            "Oct", 
                            "Nov", 
                            "Dec"
                          };

char *daysOfTheWeek[7] =  {
                            "Sun", 
                            "Mon", 
                            "Tue", 
                            "Wed", 
                            "Thu", 
                            "Fri", 
                            "Sat"
                          };

DateTime now;

//
//  Define Variables for Built In LED Flashing.
//    (Proof of Life indicator) 
//
const int   LEDOnTime   =   1;    // Time L.E.D. will remain On in msec.
const int   LEDOffTime  = 999;    // Time L.E.D. will remain Off in msec.
                                  //    LEDOnTime + LEDOffTime == 1000

bool        LEDStat     = false;   // LED Ststus false = OFF, true = ON

//-----------------------------------------------------------------------------

//
//  Define 4 x 4 Keypad
//
const byte ROWS = 4;
const byte COLS = 4;

//
//  Define Keypad Row & Column Pins on Arduino Mega 2560 Board
//
const byte ROW_PINS[ROWS] = { 23, 25, 27, 29 };
const byte COL_PINS[COLS] = { 31, 33, 35, 37 };

//
// Define values used by the panel and buttons
//
int lcd_key     = 0;
#define btnUP     '2'
#define btnLEFT   '4'
#define btnRIGHT  '6'
#define btnDOWN   '8'
#define btnALARM  'A'
#define btnSELECT 'D'
#define btnNONE    0

char BUTTONS[ROWS][COLS] = {
  { '1',  '2',  '3',  'A' },  // 1st row
  { '4',  '5',  '6',  'B' },  // 2nd row
  { '7',  '8',  '9',  'C' },  // 3rd row
  { '*',  '0',  '#',  'D' }   // 4th row
};

//
//  Define the Keypad in software as we will be using it from the above definitions.
//
Keypad keypad = Keypad(makeKeymap(BUTTONS), ROW_PINS, COL_PINS, ROWS, COLS);

//-----------------------------------------------------------------------------

//
//  Liquid Cryatal Display
//    Cursor Position Variables
//
const short rows      =  4;   // 20 x 4 LCD # of Rows
const short cols      = 20;   // 20 x 4 LCD # of Columns

// I2C Address = 27Hex, Colums = 20 and Rows = 4 for our 20x4 LCD
LiquidCrystal_I2C lcd(0x27, cols, rows);

const short csrHome   = 0;
const short csrMonth  = 5;
const short csrDay    = 9;
const short csrYear   = 15;
const short csrHour   = 1;
const short csrMinute = 4; 
const short csrSecond = 7;
const short csrAMPM   = 8;
const short csrTemp   = 10;
const short csrIsH24  = 13;
const short csrR0     = 0;
const short csrR1     = 1;
const short csrR2     = 2;
const short csrR3     = 3;

//-----------------------------------------------------------------------------
//  setup() Function
//-----------------------------------------------------------------------------
void setup () 
{
  Serial.begin( 115200 );
  while( !Serial );
  Wire.begin();
  delay(500);

  //
  // Initialize digital pin LED_BUILTIN as an output.
  //
  //  Note:  LED_BUILTIN is defined properly by the compiler
  //         for the Board being used.
  // 
  //  For Details SEE: 
  //      Help --> Referance --> pinMode
  //      Under "Digital I/O" in Arduino IDE.
  //
  pinMode( LED_BUILTIN, OUTPUT );

  pinMode( buzzerPin,   OUTPUT );

  //
  // Initialize 20 x 4 LCD display
  //
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.createChar( 0, degreeSymbol );  // Create º Symbol for LCD Shield
  lcd.setCursor( csrHome, csrR0 );    // Go to beginning of first line

  //
  // Initialize RTC interface
  //
  //    Starts I2C for DS3231 and test successful connection.
  //      true = DS3231 found.
  //      false = DS3231 NOT found.
  //
  if( !rtc.begin() ) 
  {
    //
    //  Let the world know, RTC Not Found!
    //  (Houston, we have a problem)
    //
    Serial.print( "RTC: Not Found" );
    Serial.print( "RESET->try again" );
    Serial.println();

    lcd.setCursor( csrHome,csrR0 ); // Top Line First Chatacter
    lcd.print( "RTC: Not Found" );
    lcd.setCursor( csrHome,csrR1 ); // Bottom Line First Chatacter
    lcd.print( "RESET->try again" );

    //
    //  The only ways out:
    //    - Power Failure
    //    - Reset
    //
    while( true )     // Loop till power off or reset...
    {
      LEDStat = !LEDStat; 
      digitalWrite( LED_BUILTIN, LEDStat );
      delay( 100 );

    } // End of while()

    //
    //  If logic flow reached this point, RTC is available.
    //

  } // End of if()

  //
  //  RTC lost power?
  //    true = RTC lost power.
  //    fslde = RTC is running
  //
  if( rtc.lostPower() ) 
  {
    Serial.print( "RTC: Lost Power!" );
    Serial.println();
    lcd.setCursor( csrHome, csrR0 );  // Top Line First Chatacter
    lcd.print("RTC: LostPower!");
    lcd.setCursor( csrHome, csrR1 );  // Top Line First Chatacter

    delay( 2000 );

    //
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    //
    rtc.adjust( DateTime( F(__DATE__), F(__TIME__) ) );

    //
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    //
    // rtc.adjust( DateTime( 2014, 1, 21, 3, 0, 0 ) );
    //

  } // End of if()

  printf( "============================================================\n" );
  printf( "To set RTC date/time enter YYMMDDHHMMSS as all digits where:\n" );
  printf( "  YY = last two digits of year.\n" );
  printf( "  MM = month (01-12)\n" );
  printf( "  DD = day (01-31)\n" );
  printf( "  HH = Hour of day (00-23)\n" );
  printf( "  MM = Minute of hour (00-59)\n" );
  printf( "  SS = Second (00-59)\n\n" );

  printf( "To set RTC Alarm HHMM as all digits where:\n" );
  printf( "  HH = Hour of day (00-23)\n" );
  printf( "  MM = Minute of hour (00-59)\n" );
  printf( "============================================================\n" );
  delay( 2000 );

} // End of setup()

//-----------------------------------------------------------------------------
//  loop() Function
//-----------------------------------------------------------------------------
void loop () 
{
  serialMonitorSetDateTime();

  now = rtc.now();
  int currentHour   = now.hour();
  int currentMinute = now.minute();
  int currentSecond = now.second();

  if( currentSecond != prevSecond )
  {
    prevSecond = currentSecond;

    //
    // Heart Beat: Flash Biult In LED
    //
    LEDStat = !LEDStat; 
    digitalWrite( LED_BUILTIN, LEDStat );
    
    displayTime();
    displayTemp();

    printf( "Alarm: %02d:%02d\n", RTCtime[ixAlarmHour], RTCtime[ixAlarmMinute] );

  } // End of if()

  if( currentHour == RTCtime[ixAlarmHour] && currentMinute == RTCtime[ixAlarmMinute] ) 
  {
    alarmBuzzer();
  
  } // End of if() 
  
  else 
  {
    digitalWrite(buzzerPin, LOW);
  
  } // End of else

  switch( keypad.getKey() )
  {
    case btnALARM:
      set_ALARM();
      break;

    case btnSELECT:
      set_RTC();
      break;

    default:
      break;

  } // Enf of switch()

} // End of loop()

//-----------------------------------------------------------------------------
// serialMonitorSetDateTime() Function
//-----------------------------------------------------------------------------
void serialMonitorSetDateTime( void )
{
  //
  // If we have any characters available read them here to see if
  // user wishes to set the time by entering a new time in the format:
  //    YYMMDDHHMMSS (year, month, day, hour, minute, sec)
  // Or new alarm in the format:
  //    HHMM (hour, minute)
  //
  if( Serial.available() ) 
  {
    String newDate = Serial.readStringUntil( '\r' );
    newDate.trim();
    printf( "\nReceived string: >%s<\n\n", newDate.c_str() );

    Serial.read(); // Purge 0AH Line Feed Character remaining in buffer

    //
    //  Process New Date&Time String
    //    YYMMDDHHMMSS
    //
    if( newDate.length() == 12 ) 
    {
      printf( "Correct Date&Time length!  Parsing...\n" );
      int count = sscanf( newDate.c_str(), "%2d%2d%2d%2d%2d%2d", &RTCtime[ixYears], &RTCtime[ixMonths], &RTCtime[ixDays], &RTCtime[ixHours], &RTCtime[ixMinutes], &RTCtime[ixSeconds] );
      printf( "scanned in %d values\n", count );
      printf("  Date: %04d/%02d/%02d, Time: %02d:%02d:%02d\n", RTCtime[ixYears], RTCtime[ixMonths], RTCtime[ixDays], RTCtime[ixHours], RTCtime[ixMinutes], RTCtime[ixSeconds] );
    
      if( RTCtime[ixMonths] <= 12 && RTCtime[ixDays] <= 31 && RTCtime[ixHours] <= 23 && RTCtime[ixMinutes] <= 59 && RTCtime[ixSeconds] <= 59 ) 
      {
        setNewRTC();
        printf( "New Date&Time set!\n" );
      
      } // End of if()
      
      else 
      {
        printf( "Some values are out of range, try again.\n" );
      
      } // End of else
    
    }

    //
    //  Process New Alarm String
    //    HHMM
    //
    else if( newDate.length() == 4 ) 
    {
      printf( "Correct Alarm length!  Parsing...\n" );
      int count = sscanf( newDate.c_str(), "%2d%2d", &RTCtime[ixAlarmHour], &RTCtime[ixAlarmMinute] );
      printf( "scanned in %d values\n", count );
      printf( "  Alarm: %02d:%02d\n", RTCtime[ixAlarmHour], RTCtime[ixAlarmMinute] );
    
      if( RTCtime[ixAlarmHour] <= 23 && RTCtime[ixAlarmMinute] <= 59 ) 
      {
        printf( "New Alarm set!\n" );
      
      } // End of if()
      
      else 
      {
        printf( "Some values are out of range, try again.\n" );
      
      } // End of else

    } // End of else if()
    
    else 
    {
      printf( "Wrong length for YYMMDDHHMMSS or HHMM.  %d characters\n", newDate.length() );
    
    } // End of else

    delay( 2000 );

  } // End of if()

} // End of serialMonitorSetDateTime()

//-----------------------------------------------------------------------------
// alarmBuzzer() Function
//-----------------------------------------------------------------------------
void alarmBuzzer() 
{
  // Example alarm buzzer sequence
  for( int i = 0; i < 5; i++ ) 
  {
    Serial.println(" Buzz..." );

    digitalWrite( buzzerPin, HIGH );
    delay( 100 );
    digitalWrite( buzzerPin, LOW );
    delay( 100 );
  
  } // End of for()

} // End of alarmBuzzer()

//-----------------------------------------------------------------------------
//  Set and Display New RTC Date and Time
//-----------------------------------------------------------------------------
//
//  This:
//    Updates the RTC's Clock from our working Date/Time Structure 
//    Displays the new Date and Time
//
void setNewRTC()
{
  rtc.adjust(DateTime(  RTCtime[ixYears], 
                        RTCtime[ixMonths], 
                        RTCtime[ixDays], 
                        RTCtime[ixHours], 
                        RTCtime[ixMinutes], 
                        RTCtime[ixSeconds] ));
  now = rtc.now();       // Get current Date and Time from RTC
  displayTime();

} // End of setNewRTC()

//-----------------------------------------------------------------------------
// Display Time
//-----------------------------------------------------------------------------
void displayTime()
{
  Serial.print( daysOfTheWeek[now.dayOfTheWeek()] );
  Serial.print( " " );

  Serial.print( MonthName[now.month() - 1] );
  Serial.print( " " );

  lcd.setCursor( csrHome, csrR0 );    // Top Line First Chatacter
  lcd.print( daysOfTheWeek[now.dayOfTheWeek()] );
  lcd.print( " " );

  lcd.print( MonthName[now.month() - 1] );
  lcd.print( " " );

  //
  //  Always display 2 characters for Day
  //    If Day < 10 we display a space
  //
  if( now.day() < 10 )
  {
    Serial.print( " " );

    lcd.print( " " );

  } // Enf of if()

  Serial.print( now.day(), DEC );
  Serial.print( ", " );

  lcd.print( now.day(), DEC );
  lcd.print( ", " );

  Serial.print( now.year(), DEC );
  Serial.print( " " );

  lcd.print( now.year(), DEC );

  //
  //  Top line of display is complete
  //    Prepair for Bottom line
  //
  lcd.setCursor( csrHome,csrR1 ); // Bottom Line First Chatacter

  //
  //  Display Hours
  //
  //
  //  Save hours to be adjusted for 12 or 24 hour clock
  //
  printHours = now.hour();

  //
  //  Adjust Hours to print for 12 hour clock.
  //
  if( !RTCtime[ixIsH24] )
  {
    if( now.hour() > 12)
      printHours -= 12;

    if( printHours == 0 )
      printHours = 12;

  } // End of if

  //
  //  Always display 2 characters for Hour
  //    If Hour < 10 we display a 0
  //
  if( printHours < 10 )
  {
    Serial.print( "0" );

    lcd.print( "0" );

  } // End of if()

  Serial.print( printHours, DEC );
  Serial.print( ':' );

  lcd.print( printHours, DEC );
  lcd.print( ':' );

  //
  //  Always display 2 characters for Minute
  //    If Minute < 10 we display a 0
  //
  if( now.minute() < 10 )
  {
    Serial.print( "0" );

    lcd.print( "0" );

  } // End of if()

  Serial.print( now.minute(), DEC );
  Serial.print( ':' );

  lcd.print( now.minute(), DEC );
  lcd.print( ':' );

  //
  //  Always display 2 characters for Second
  //    If Second < 10 we display a 0
  //
  if( now.second() < 10 )
  {
    Serial.print( "0" );

    lcd.print( "0" );
  
  } // End of if()

  Serial.print( now.second(), DEC );

  lcd.print( now.second(), DEC );

  //
  //  Display AM/PM indicator id 12 hour clock
  //
  if( !RTCtime[ixIsH24] )
  {
    if( now.hour() > 11 )
    {
      Serial.print( "P " );
      lcd.print( "P " );

    } // End of if()
  
    else 
    {
      Serial.print( "A " );
      lcd.print( "A " );

    } // End of else

  } // End of if

} // End of displayTime()

//-----------------------------------------------------------------------------
// Display Alarm
//-----------------------------------------------------------------------------
void displayAlarm()
{
  //
  //  Top line of display is complete
  //    Prepair for Bottom line
  //
  lcd.setCursor( csrHome, csrR1 ); // Bottom Line First Chatacter

  //
  //  Display Alarm Hours (24 Hour Format)
  //
  //  Always display 2 characters for Hour
  //    If Hour < 10 we display a 0
  //
  if( RTCtime[ixAlarmHour] < 10 )
  {
    Serial.print( "0" );

    lcd.print( "0" );

  } // End of if()

  Serial.print( RTCtime[ixAlarmHour], DEC );
  Serial.print( ':' );

  lcd.print( RTCtime[ixAlarmHour], DEC );
  lcd.print( ':' );

  //
  //  Always display 2 characters for Minute
  //    If Minute < 10 we display a 0
  //
  if( RTCtime[ixAlarmMinute] < 10 )
  {
    Serial.print( "0" );

    lcd.print( "0" );

  } // End of if()

  Serial.println( RTCtime[ixAlarmMinute], DEC );

  lcd.print( RTCtime[ixAlarmMinute], DEC );

} // End of displayAlarm()

//-----------------------------------------------------------------------------
// Display Temp in ºF
//-----------------------------------------------------------------------------
void displayTemp()
{
  lcd.setCursor( csrTemp, csrR1 );   // Bottom Line First Chatacter

  //
  //  In the United States, we normally use ºF 
  //
  Serial.print( int((rtc.getTemperature() * 1.8) + 32.0) );
  Serial.print( "°" );   // write custom character: º Symbol
  Serial.print( "F  " );
  lcd.print( int( ( rtc.getTemperature() * 1.8 ) + 32.0 ) );
  lcd.write( byte(0) );   // write custom character: º Symbol
  lcd.print( "F  " );

  //
  //  For ºC, use this and comment out above 
  //
/*
  Serial.print( int(rtc.getTemperature()) );
  Serial.print("°");   // write custom character: º Symbol
  Serial.print("C  ");

  lcd.print( int(rtc.getTemperature()) );
  lcd.write( byte(0) );   // write custom character: º Symbol
  lcd.println( "C  " );
*/

} // End of displayTemp()

//-----------------------------------------------------------------------------
//  Set Alarm Time Hourd & Minutes
//-----------------------------------------------------------------------------
//
//  The program flow hangs out here till the SELECT Button is pressed
//  indicating that the desired Time has been set for the Alarm.
//
void set_ALARM( void )
{
  short adjust = ixAlarmHour;  //  adjust is an indix into the RTCtime[] array.
                                //  the first element we adjust is Months.  
  Serial.println();
  lcd.clear();
  lcd.print( "Set Alarm:" );
  Serial.print( "Set Alarm:" );
  lcd.setCursor( csrHome, csrR1 );
  displayAlarm();

  //
  //  Finish Pre-Initializing the Display
  //
  lcd.setCursor(csrHour, csrR1 ); // Position Cursor 1st Row, 2nd Column 
                                  //    to adjust month initially.
  lcd.cursor();                   //  Turn Cursor On 
                                  //    (Normally off when not setting clock)
  lcd.blink();                    //  Blink the Cursor so it's easy to see.
  
  //
  //  Loop till Select Button pushed 
  //
  while( true )
  {
    delay( 250 );                 // Debounce keypad by delaying

    lcd_key = keypad.getKey();    // Get selected Button (if any)
                                  //    SELECT
                                  //    LEFT
                                  //    UP
                                  //    DOWN
                                  //    RIGHT
                                  //    NONE
    //
    //  Process Selected Button, if any
    //
    switch( lcd_key )
    {
    case btnRIGHT:                // RIGHT Button
    case btnLEFT:                 // LEFT Button

      //
      //  Processing LEFT and RIGHT Buttons is very similar.
      //  Switch within Switch reduces code.
      //
      switch( lcd_key )
      {
      case btnRIGHT:              // RIGHT Button
          if( adjust < ixAlarmMinute )
            adjust = ixAlarmMinute;
          else
            adjust = ixAlarmHour;
          break;

      case  btnLEFT:              // LEFT Button
          if( adjust > ixAlarmHour )
            adjust = ixAlarmHour;
          else
            adjust = ixAlarmMinute;
          break;

      } // Enf of switch()

      //
      //  The adjust variable has been altered by the LEFT and RIGHT Buttons.
      //  We now move the Blinking Cursor to the field the user wnts to adjust.
      //
      switch( adjust )
      {
      case ixAlarmHour:
            lcd.setCursor( csrHour, csrR1 );
            break;

      case ixAlarmMinute:
            lcd.setCursor( csrMinute, csrR1 );
            break;

      } // End of switch

      //
      //  At this point the Blinking Cursor is positiones
      //    to the field to be adjusted.
      //
      //  Processing of LEFT or RIGHT Button is complete
      //
      break;

    case btnUP:                   // UP Button
    case btnDOWN:                 // DOWN Button

      //
      //  Processing UP and DOWN Buttons is very similar.
      //  Switch within Switch reduces code.
      //
      switch(lcd_key)
      {
      case btnUP:                 // UP Button
            RTCtime[adjust]++;
            break;

      case btnDOWN:               // DOWN Button
            RTCtime[adjust]--;
            break;

      } // End of switch

      //
      //  We have adjusted the RTC time array with the UP or Down Button.
      //  This quite possibly created Out Of Range errors in the selected fields.
      //
      //  The code below:
      //    - corrects the errors depending on the Field.
      //    - rewrites the RTC Display.
      //    - repositions the Blinking Cursor to its proper location.
      //
      switch( adjust )
      {
      case ixAlarmHour:              // Hours
            if( RTCtime[ixAlarmHour] > 23 )
              RTCtime[ixAlarmHour] = 0;
            else if( RTCtime[ixAlarmHour] < 0 )
              RTCtime[ixAlarmHour] = 23;

            displayAlarm();
            lcd.setCursor( csrHour , csrR1 );
            break;

      case ixAlarmMinute:              // Minutes
            if( RTCtime[ixAlarmMinute] > 59 )
              RTCtime[ixAlarmMinute] = 0;
            else if( RTCtime[ixAlarmMinute] < 0 )
              RTCtime[ixAlarmMinute] = 59;

            displayAlarm();
            lcd.setCursor( csrMinute , csrR1 );
            break;

      } // End of switch

    break;

    //
    //  Someone pushed the SELECT Button
    //
    //    Time to finish and go home. 
    //      Return to caller
    //
    case btnSELECT:               // SELECT Button
        Serial.println();
        lcd.noCursor();
        lcd.noBlink();
        lcd.clear();
        displayTime();
        return;

    //
    //  No Button wes pushed...
    //
    //    Loop through and try again.
    //    This case is used far more than any other.
    //    Yet, is does the least amount of work.
    //
    case btnNONE:                 // NO Button
         break;

    } // End of switch()

  } // End of while()

} // End of set_ALARM()

//-----------------------------------------------------------------------------
//  Set Real Time Clock
//-----------------------------------------------------------------------------
//
//  The program flow hangs out here till the SELECT Button is pressed
//  indicating that the desired Date and Time have been set in the RTC time.
//
void set_RTC()
{
  short adjust = ixMonths;    //  adjust is an indix into the RTCtime[] array.
                              //  the first element we adjust is Months.

  //
  // Load RTCtime[] with passed in time.
  //
  RTCtime[ixMonths]  = now.month();
  RTCtime[ixDays]    = now.day();
  RTCtime[ixYears]   = now.year();
  RTCtime[ixHours]   = now.hour();
  RTCtime[ixMinutes] = now.minute();
  RTCtime[ixSeconds] = now.second();
//  RTCtime[ixIsPM]
//  RTCtime[ixIsH24]

  lcd.clear();                // LCD Cursor to Home Position.
  lcd.print( "Setting RTC" ); // Let the user know, 
                              //   change in modes of opperation
  delay( 1000 );              // Wait so they can see 

  //
  //  We use the same basic time display when setting the time.
  //  We messed it up above so now we refresh it.
  //
  displayTime();         // Display current RTC time

  //
  //  This adds a 12H or 24H to the time display to indicate 12 or 24 hour clock
  //
  lcd.setCursor( csrIsH24 , csrR1 );    // Position cursor on 2nd Row 14th Column

  if( RTCtime[ixIsH24] )
    lcd.print( "24H");        // Display 24H if 24 Hour Clock
  else
    lcd.print( "12H");        // Display 12H if 12 Hour Clock

  //
  //  Finish Pre-Initializing the Display
  //
  lcd.setCursor( csrMonth, csrR0 ); // Position Cursor 1st Row, 2nd Column 
                                    //    to adjust month initially.
  lcd.cursor();                     //  Turn Cursor On 
                                    //    (Normally off when not setting clock)
  lcd.blink();                      //  Blink the Cursor so it's easy to see.

  //
  //  Loop till Select Button pushed 
  //
  while( true )
  {
    delay( 250 );                 // Debounce keypad by delaying

    lcd_key = keypad.getKey();    // Get selected Button (if any)
                                  //    SELECT
                                  //    LEFT
                                  //    UP
                                  //    DOWN
                                  //    RIGHT
                                  //    NONE
    //
    //  Process Selected Button, if any
    //
    switch( lcd_key )
    {
    case btnRIGHT:                // RIGHT Button
    case btnLEFT:                 // LEFT Button

      //
      //  Processing LEFT and RIGHT Buttons is very similar.
      //  Switch within Switch reduces code.
      //
      switch( lcd_key )
      {
      case btnRIGHT:              // RIGHT Button
          if( adjust < ixIsH24 )
            adjust++;
          else
            adjust = ixMonths;
          break;

      case  btnLEFT:              // LEFT Button
          if( adjust == ixIsH24 )
            adjust--;
            
          if( adjust > ixMonths )
            adjust--;
          else
            adjust = ixIsH24;
          break;

      } // Enf of switch()

      //
      //  The adjust variable has been altered by the LEFT and RIGHT Buttons.
      //  We now move the Blinking Cursor to the field the user wnts to adjust.
      //
      switch( adjust )
      {
      case ixMonths:               // Months
           lcd.setCursor( csrMonth , csrR0 );
           break;

      case ixDays:                 // Days
           lcd.setCursor( csrDay , csrR0 );
           break;

      case ixYears:                // Years
           lcd.setCursor( csrYear , csrR0 );
           break;

      case ixHours:                // Hours
           lcd.setCursor( csrHour , csrR1 );
           break;

      case ixMinutes:              // Minutes
           lcd.setCursor( csrMinute , csrR1 );
           break;

      case ixSeconds:              // Seconds
           lcd.setCursor( csrSecond , csrR1 );
           break;

      case ixIsPM:                 // AM/PM Indicator
            adjust++;
//           lcd.setCursor( csrAMPM , csrR1 );
//           break;

      case ixIsH24:                // 12/24 Hour Clock Indicator
           lcd.setCursor( csrIsH24, csrR1 );
           break;

      } // End of switch

      //
      //  At this point the Blinking Cursor is positiones
      //    to the field to be adjusted.
      //
      //  Processing of LEFT or RIGHT Button is complete
      //
      break;

    case btnUP:                   // UP Button
    case btnDOWN:                 // DOWN Button

      //
      //  Processing UP and DOWN Buttons is very similar.
      //  Switch within Switch reduces code.
      //
      switch( lcd_key )
      {
      case btnUP:                 // UP Button
            RTCtime[adjust]++;
            break;

      case btnDOWN:               // DOWN Button
            RTCtime[adjust]--;
            break;

      } // End of switch

      //
      //  We have adjusted the RTC time array with the UP or Down Button.
      //  This quite possibly created Out Of Range errors in the selected fields.
      //
      //  The code below:
      //    - corrects the errors depending on the Field.
      //    - rewrites the RTC Display.
      //    - repositions the Blinking Cursor to its proper location.
      //
      switch( adjust )
      {
      case ixMonths:               // Months
            if( RTCtime[ixMonths] > 12 )
              RTCtime[ixMonths] = 1;
            else if( RTCtime[ixMonths] < 1 )
              RTCtime[ixMonths] = 12;

            setNewRTC();
            lcd.setCursor( csrMonth , csrR0 );
            break;

      case ixDays:                // Days
                                  //
                                  //  WARNING: Needs Leap Day logic added
                                  //
            if( RTCtime[ixDays] > (MonthLen[RTCtime[ixMonths] -1 ] ) )
              RTCtime[ixDays] = 1;
            else if( RTCtime[ixDays] < 1 )
              RTCtime[ixDays] = (MonthLen[RTCtime[ixMonths] -1 ] );
              
            setNewRTC();
            lcd.setCursor( csrDay , csrR0 );
            break;

      case ixYears:                // Years
            setNewRTC();
            lcd.setCursor( csrYear , csrR0 );
            break;

      case ixHours:              // Hours
            if( RTCtime[ixHours] > 23 )
              RTCtime[ixHours] = 0;
            else if( RTCtime[ixHours] < 0 )
              RTCtime[ixHours] = 23;

            setNewRTC();
            lcd.setCursor( csrHour , csrR1 );
            break;

      case ixMinutes:              // Minutes
            if( RTCtime[ixMinutes] > 59 )
              RTCtime[ixMinutes] = 0;
            else if( RTCtime[ixMinutes] < 0 )
              RTCtime[ixMinutes] = 59;

            setNewRTC();
            lcd.setCursor( csrMinute , csrR1 );
            break;

      case ixSeconds:              // Seconds
            if( RTCtime[ixSeconds] > 59 )
              RTCtime[ixSeconds] = 0;
            else if( RTCtime[ixSeconds] < 0 )
              RTCtime[ixSeconds] = 59;

            setNewRTC();
            lcd.setCursor( csrSecond , csrR1 );
            break;

//      case ixIsPM:                 // Is AM/PM?
//            if( RTCtime[ixIsPM] > true )
//              RTCtime[ixIsPM] = false;
//            else if( RTCtime[ixIsPM] < false)
//              RTCtime[ixIsPM] = true;
//
//            setNewRTC();
//            lcd.setCursor( csrAMPM , csrBottom );
//
//            break;

      case ixIsH24:                // Is 12/24 Hour Clock?
            if( RTCtime[ixIsH24] > true )
              RTCtime[ixIsH24] = false;
            else if( RTCtime[ixIsH24] < false )
              RTCtime[ixIsH24] = true;

            setNewRTC();
            lcd.setCursor( csrIsH24, 1 );

            if( RTCtime[ixIsH24] )
              lcd.print( "24H" );
            else
              lcd.print( "12H" );

            lcd.setCursor( csrIsH24, csrR1 );

            break;

      } // End of switch

    break;

    //
    //  Someone pushed the SELECT Button
    //
    //    Time to finish and go home. (Return to caller)
    //
    case btnSELECT:               // SELECT Button
         rtc.adjust( DateTime( RTCtime[ixYears], 
                               RTCtime[ixMonths], 
                               RTCtime[ixDays], 
                               RTCtime[ixHours], 
                               RTCtime[ixMinutes], 
                               RTCtime[ixSeconds] ) );
         lcd.noCursor();
         lcd.noBlink();
         lcd.clear();
         displayTime();
         return;

    //
    //  No Button wes pushed...
    //
    //    Loop through and try again.
    //    This case is used far more than any other.
    //    Yet, is does the least amount of work.
    //
    case btnNONE:                 // NO Button
         break;

    } // End of switch()

  } // End of while()

} // End of set_RTC()
