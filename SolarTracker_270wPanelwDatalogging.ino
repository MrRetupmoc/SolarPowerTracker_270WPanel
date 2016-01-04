/*------------------------------------------------------------------------------------------------------
                              Author : Evan Richinson aka MrRetupmoc42
               
Adalogger Solar Panel Power Tracker to SD Card
   Stores Calculated Values from Analog Voltage Input
   Custom RTC Called MTC ( MicroProcessor Time Clock ), Tuned?
   When Charging we Track this Data the Most, Idle Panel Track Every Min ( Track Sun Levels )
   
   Future Idea's
  
   Peek Power with MTC Timestamp ( Peek Power Days )
   Eventually wanting to Have this setup run a 1 Axis Tracking
   
December 30th 2015 : Data Poll, Calculations and Storage
December 31st 2015 : Cleanup of Storage and Started MultiStore
January 1st 2016   : "MTC ( MicroProcessor Time Clock )" and Max Time Values
January 2nd 2016   : Storage Update for MTC, Now MultiStores S,M,H,D and Y, 
                      Linerization Added to Calculations, Sleep/Status LED Updated and
                      Started Working on Averages for Miniute Update ( Playing with Arrays )
January 3rd 2016   : Fought With the Adalogger not taking new code.... :(
January 4th 2016   : Reset Fixed Adalogger to take code again...
                      Updated some Logic and Linerization with a Mega (Converted to a 3v3 Analog Pin Controller ),
                      Averaging Code Fails ( Reverted for now but still working on it )
                   
-------------------------------------------------------------------------------------------------------*/

//SD Card Setup and File Delimiters
#include <SPI.h>
#include <SD.h>
  const int chipSelect = 4;
  String dataString = "";
  String Split = "/";
  String End = ".";

#include "DHT.h"

//Pin Setup and Calculation Data
#define StatusLed 13
#define VBatLipo A9
#define VBatLeadAcid A2
#define VSolar A0
#define ISolar A1
#define TempHumidity 6
  DHT dht(TempHumidity, DHT22);
  
  float MeasuredVBatLipo;
  float MeasuredVBatLeadAcid;
  float MeasureVSolarPanel;
  float MeasureISolarPanel;
  float MeasureWSolarPanel;
  float MeasureTemperature;
  float MeasureHumidity;

  float UnderVBatLipo = 3.300;               //Set Under Voltage Lipo
  float UnderVBatLeadAcid = 10.500;          //Set Under Voltage Lead Acid

  bool LipoUnderChargedVoltage = false;      //
  bool LeadAcidUnderChargedVoltage = false;  //
  bool PanalUnderChargeVoltage = false;      //Cloudy or Night Time?
  bool PanalAtChargeVoltage = false;         //Charging the Battery
  bool PanalOverChargeVoltage = false;       //Sunny But Don't Need to Charge

  bool Debug = false;
  int VoltageOffset = 1;

  //This is the "MTC ( MicroProcessor Time Clock )" Setup
  int StatusFlashTimeOffset = 250;                  //mS, +250mS for LED Flash
  int CalcTimeOffset = (1000 - (StatusFlashTimeOffset + 9)); //mS, The Offset to Correct for a 1 Second Unit of Time
  
  int SecondCount = -1;      //-1 --> Init Print the Data Lines Names to .TXT
  int MinuteCount = 0;
  int HourCount = 0;
  int DayCount = 0;
  int YearCount = 0;

  //Max Time Values, Changes Size of Capture Arrays
  const int SecondCount_Max = 60;  //60 Sec per Min
  const int MinuteCount_Max = 60; //60 Min per Hour
  const int HourCount_Max = 24;    //24 Hours per Day
  const int DayCount_Max = 365;    //365 Days per Year
  const int YearCount_Max = 10;    //10 Years worth of Logs....

  //Storage Array to Create the Average Values From
  const int dataspots = 4;

  //Array(s)
  //First DataSposts, (LipoBat_V/LeadAcidBat_V/SolarPanel_V/SolarPanel_I/SolarPanel_W/Temperature/Humidity)
  //Second CountperUnitTime, (Min/Hour/Day/Year and Decade?)
  int CapturesperMinuite[dataspots][SecondCount_Max];
  int CapturesperHour[dataspots][MinuteCount_Max];
  int CapturesperDay[dataspots][HourCount_Max]; 
  int CapturesperYear[dataspots][DayCount_Max];
//int CapturesperDecade[dataspots][YearCount_Max];

  //Array(s) of Averages
  ///First DataSposts, (LipoBat_V/LeadAcidBat_V/SolarPanel_V/SolarPanel_I/SolarPanel_W/Temperature/Humidity)
  int MinuiteAverage[dataspots];
  int HourAverage[dataspots];
  int DayAverage[dataspots];
  int YearAverage[dataspots];

  //Averaging Counts
  int MinAveragingValue = 0;

//Peek Values ( Dunno Yet )


//---------------------------------------------------------------------------------------------------------------------------------------

void setup() {
  pinMode(StatusLed, OUTPUT);
  Serial.begin(9600);
  dht.begin();

  digitalWrite(StatusLed, HIGH);
  delay(5000);
  digitalWrite(StatusLed, LOW);

  if (Serial) Serial.println("Full Mode Initialiazation");
    if (!SD.begin(chipSelect)) {
      if (Serial) Serial.println("Card failed, or not present");
      return;
    }
  if (Serial) Serial.println("Card Save Enabled.");
  
  digitalWrite(StatusLed, HIGH);
  delay(2000);
  digitalWrite(StatusLed, LOW);
  delay(2000);
}

//-------------------------------------------------------------------------------------------------------------------------------------

void loop() {
  //Start Indication of a Poll
  digitalWrite(StatusLed, HIGH);
  delay(StatusFlashTimeOffset);
  
  //Grab Values from Analog Input
  MeasuredVBatLipo = analogRead(VBatLipo);
  MeasuredVBatLeadAcid = analogRead(VBatLeadAcid);
  MeasureVSolarPanel = analogRead(VSolar);
  MeasureISolarPanel = analogRead(ISolar);
  
  //Adafruit says to poll every 2 seconds or more.... what will 1 second do? break it?
  if (SecondCount%2 == 0) {
    MeasureTemperature = dht.readTemperature(); //30;//
    MeasureHumidity = dht.readHumidity(); //40;//
  }


  
  //Calculate to Correct Values
  //Internal Resistor Divider Setup 2x
  MeasuredVBatLipo *= 2; //Resistor Divider Inside is 2x
  MeasuredVBatLipo *= 3.3; //WORKS Convert to Voltage of Board
  MeasuredVBatLipo /= 1024; //WORKS Convert the Counts per Voltage
  MeasuredVBatLipo *= 1.01; //% Error Offset


  //External Resistor Divider Setup 4x
  //  Counts > Readout = Correction ( int to V ), 3.3v / 1024 = 3.22mV per Step
    //584    --> 10.73v
    //635    --> 11.69v
    //737    --> 13.59v
  MeasuredVBatLeadAcid *= (0.01868 * (5 / 3.3)); //m, Tuned with a Mega, 5v per Analog Pin not 3v3...
  MeasuredVBatLeadAcid -= 0.1783; //b


  //External Autopilot 50V 180A Version
    //50V/180A = 63.69mV / Volt, 18.30mV / Amp.
    //3.3v / 1024 = 3.22mV per Step

  //Solar Panel Voltage ( With Lineraization )
    //63.69mV / Volt / 3.22 = 19.7795
  //  Counts > Readout = Correction ( int to V ), 3.3v / 1024 = 3.22mV per Step
    //0    --> 00.00v
    //310  --> 22.60v
  MeasureVSolarPanel *= (0.07290 * (5 / 3.3)); //m, Tuned with a Mega, 5v per Analog Pin not 3v3...
  //MeasureVSolarPanel += 0; //b

  //Solar Panel Current ( With Lineraization )
    //18.30mV / Amp / 3.22 = 5.6832
  //  Counts > Readout = Correction ( int to I ), 3.3v / 1024 = 3.22mV per Step
    //3    --> 01.30a
    //45   --> 12.00a
  MeasureISolarPanel *= (0.2548 * (5 / 3.3)); //m, Tuned with a Mega, 5v per Analog Pin not 3v3...
  MeasureISolarPanel += 0.5357; //b
  

  //Panel Wattage Calulation ( Simple )
  MeasureWSolarPanel = MeasureVSolarPanel * MeasureISolarPanel;


  //Logic Statements ---------------------------------------------------------------------------------------------------------------------

  if (MeasuredVBatLipo <= UnderVBatLipo) LipoUnderChargedVoltage = true;
  else LipoUnderChargedVoltage = false;

  if (MeasuredVBatLeadAcid <= UnderVBatLeadAcid) LeadAcidUnderChargedVoltage = true;
  else {
    LeadAcidUnderChargedVoltage = false;
  }

  if (MeasureVSolarPanel < (MeasuredVBatLeadAcid - VoltageOffset)) {
    PanalUnderChargeVoltage = true;
    PanalAtChargeVoltage = false;
    PanalOverChargeVoltage = false;
  }
  else if(MeasureVSolarPanel >= (MeasuredVBatLeadAcid + VoltageOffset) && MeasureVSolarPanel <= (MeasuredVBatLeadAcid - VoltageOffset)) {
    PanalUnderChargeVoltage = false;
    PanalAtChargeVoltage = true;
    PanalOverChargeVoltage = false;
  }
  else if (MeasureVSolarPanel > (MeasuredVBatLeadAcid + VoltageOffset)) {
    PanalUnderChargeVoltage = false;
    PanalAtChargeVoltage = false;
    PanalOverChargeVoltage = true;
  }
  if (MeasureVSolarPanel <= 0  || MeasureVSolarPanel <= 0 && LeadAcidUnderChargedVoltage) {
    PanalUnderChargeVoltage = false;
    PanalAtChargeVoltage = false;
    PanalOverChargeVoltage = false;
  }
  

  //Serial Out the Values ------------------------------------------------------------------------------------------------------------------
  if (Serial) {
    Serial.println("");
    Serial.println("********************************");
    Serial.print("Lipo Bat Voltage : " );
    Serial.println(MeasuredVBatLipo);
      if(LipoUnderChargedVoltage) Serial.println("Lipo Backup is Under Charged, Sleeping");
      else Serial.println("Lipo Backup is Good to GO");
    Serial.println("-------------------------------");
    Serial.print("Lead Acid Bat Voltage : " );
    Serial.println(MeasuredVBatLeadAcid);
      if(LeadAcidUnderChargedVoltage) Serial.println("LeadAcid Backup is Under Charged...");
      else if(MeasureISolarPanel > 0 && !LeadAcidUnderChargedVoltage) Serial.println("LeadAcid Backup is Charging");
      else Serial.println("LeadAcid Backup is Ready");
    Serial.println("-------------------------------");
    Serial.print("Solar Panel Voltage : " );
    Serial.println(MeasureVSolarPanel);
    Serial.print("Solar Panel Current : " );
    Serial.println(MeasureISolarPanel);
    Serial.print("Solar Panel Wattage : " );
    Serial.println(MeasureWSolarPanel);
      if(PanalUnderChargeVoltage) Serial.println("Not Enough Voltage to Charge");
      else if(PanalAtChargeVoltage) Serial.println("Voltage at Charge Level");
      else if(PanalOverChargeVoltage) Serial.println("Ready to Charge / Done Charging Backup Battery");
      else Serial.println("No Solar Panel Found");
    Serial.println("-------------------------------");
    Serial.println("The Temperature is : " + String(MeasureTemperature) + ". And Humidity is : " + String(MeasureHumidity) + ".");
    Serial.println("********************************");
  }
  
  //Datalog Spreadsheet Setup ------------------------------------------------------------------------------------------------------------------

  if(SecondCount < 0) { //Startup Print Values?
  //Datalog Spreadsheet Setup
  dataString = "Year" + Split + "Day" + Split + "Hour" + Split + "Minute" + Split + 
      "Second" + Split + "LipoBat_V" + Split + "LeadAcidBat_V" + Split + "SolarPanel_V" + Split + 
      "SolarPanel_I" + Split + "SolarPanel_W" + Split + "Temperature" + Split + "Humidity" + End;
  }
  else { //Normal Operation
  //Storage Array :   Year,                       Day,                       Hour,                       Minute,
  dataString = String(YearCount) + Split + String(DayCount) + Split + String(HourCount) + Split + String(MinuteCount) + Split + 
    //     Second,                       LipoBat_V,                         LeadAcidBat_V,                         SolarPanel_V,
    String(SecondCount) + Split + String(MeasuredVBatLipo) + Split + String(MeasuredVBatLeadAcid) + Split + String(MeasureVSolarPanel) + Split +
    //     SolarPanel_I,                        SolarPanel_W,                        Temperature,                         Humidity.
    String(MeasureISolarPanel) + Split + String(MeasureWSolarPanel) + Split + String(MeasureTemperature) + Split + String(MeasureHumidity) + End;
  }

  //Datalog All Values to One File for Second Based Updates
  if (SecondCount < SecondCount_Max) { //Second Tick Capture
    //Save Data we Captured to File
    File datalogFile_Sec = SD.open("sec.txt", FILE_WRITE);
    
    if (datalogFile_Sec) {
      datalogFile_Sec.println(dataString);
      datalogFile_Sec.close();
      if (Serial) Serial.println("Updated Second_Datalog.txt");
      if (Serial && Debug) Serial.println(dataString);
    }
    else if (Serial) Serial.println("Error Opening Second_Datalog.txt");
/*
    //Save to the Array for Averaging Later
    CapturesperMinuite[0][SecondCount] = MeasuredVBatLipo; //Dataspot Save x value
    CapturesperMinuite[1][SecondCount] = MeasuredVBatLeadAcid; //Dataspot Save x value
    CapturesperMinuite[2][SecondCount] = MeasureVSolarPanel; //Dataspot Save x value
    CapturesperMinuite[3][SecondCount] = MeasureISolarPanel; //Dataspot Save x value
    CapturesperMinuite[4][SecondCount] = MeasureWSolarPanel; //Dataspot Save x value
    CapturesperMinuite[5][SecondCount] = MeasureTemperature; //Dataspot Save x value
    CapturesperMinuite[6][SecondCount] = MeasureHumidity; //Dataspot Save x value
    
    MinAveragingValue += 1;
    */
  }
  
  //End Data Poll ------------------------------------------------------------------------------------------
  digitalWrite(StatusLed, LOW);
  
  if (MeasureISolarPanel > 0 && PanalAtChargeVoltage && !LipoUnderChargedVoltage || MeasureISolarPanel > 0 && PanalOverChargeVoltage && !LipoUnderChargedVoltage || PanalAtChargeVoltage && !LipoUnderChargedVoltage) {
    delay(CalcTimeOffset); //Should be Charging, Watch this Data Closely
    SecondCount += 1;
  }
  else if (PanalOverChargeVoltage && !LipoUnderChargedVoltage) {
    delay(CalcTimeOffset); //Whats the Sun Levels Like?
    EndDelay(4);
    SecondCount += 5;
  }
  else if (PanalUnderChargeVoltage && !LipoUnderChargedVoltage) {
    delay(CalcTimeOffset); //Still want to know what the sun levels are like when low
    EndDelay(14);
    SecondCount += 14;
  }
  else if (!PanalUnderChargeVoltage && !PanalAtChargeVoltage && !PanalOverChargeVoltage && !LipoUnderChargedVoltage) {
    delay(CalcTimeOffset); //5min, 1s Datalog Feed if not Charging and Above 0V ( Cloudy / Night Mode )
    EndDelay(29);
    SecondCount += 30;
  }
  else {
    delay(CalcTimeOffset);  //I Dunno what the stare of the System is...
    EndDelay(59);
    SecondCount += SecondCount_Max;
  }


  
  //Set Some Loosed Based MTC Time for Graphing / Keeping Time
  if (SecondCount >= SecondCount_Max) { //Minute Update, Second Reset
    SecondCount -= SecondCount_Max;
    MinuteCount += 1;
/*
    //DataLog Values to the Minute Based File
    //Average Min Values 
    for(int i = 0; i < dataspots; i++) {
      for(int j = 0; j < SecondCount_Max; i++) {
        MinuiteAverage[i] += CapturesperMinuite[i][j]; //Save all Values to Average Array
        CapturesperMinuite[i][j] = 0; //Clean out the Sec Average Array ( Don't Dirty up the Averages Later )
      }
      MinuiteAverage[i] = MinuiteAverage[i] / MinAveragingValue; //Make the Average the Average... ( Only Works if Data is Every Second... )
    }
    MinAveragingValue = 0; // Reset the Average

    //Average String Setup
    String MinAveragedataString = String(MinuiteAverage[0]) + Split + String(MinuiteAverage[1]) + Split + String(MinuiteAverage[2]) + Split + 
      String(MinuiteAverage[3]) + Split + String(MinuiteAverage[4]) + Split + String(MinuiteAverage[5]) + Split + String(MinuiteAverage[6]) + Split + "Average" + End;
*/
    //Save Data we Captured to File
    File datalogFile_Min = SD.open("min.txt", FILE_WRITE);
    if (datalogFile_Min) {
      datalogFile_Min.println(dataString);
      //datalogFile_Min.println(MinAveragedataString);
      datalogFile_Min.close();
      if (Serial) {
        Serial.println("Updated Minuite_Datalog.txt");
        //Serial.println("Average is : " + MinAveragedataString);
      }
    }
    else if (Serial) Serial.println("Error Opening Minuite Datalog.txt");

    //Save to the Array for Averaging Later
  }
  if (MinuteCount >= MinuteCount_Max) { //Hour Update, Minute Reset
    MinuteCount -= MinuteCount_Max;
    HourCount += 1;

    //DataLog Values to the Hour Based File
    //Average Min Values 

    //Save Data we Captured to File
    File datalogFile_Hour = SD.open("hour.txt", FILE_WRITE);
      if (datalogFile_Hour) {
        datalogFile_Hour.println(dataString);
        datalogFile_Hour.close();
        if (Serial) Serial.println("Updated Hour_Datalog.txt");
      }
      else if (Serial) Serial.println("Error Opening Minuite Datalog.txt");
      //Save to the Array for Averaging Later

      //CapturesperHour[SecondCount, Dataspot] = 0;
  }
  if (HourCount >= HourCount_Max) { //Day Update, Hour Reset
    HourCount -= HourCount_Max;
    DayCount += 1;

    //DataLog Values to the Day Based File
    //Average Min Values 

    //Save Data we Captured to File
    File datalogFile_Day = SD.open("day.txt", FILE_WRITE);
    if (datalogFile_Day) {
      datalogFile_Day.println(dataString);
      datalogFile_Day.close();
      if (Serial) Serial.println("Updated Day_Datalog.txt");
    }
    else if (Serial) Serial.println("Error Opening Day Datalog.txt");
    //Save to the Array for Averaging Later

    //CapturesperYear[SecondCount, Dataspot] = 0;
  }
  if (DayCount >= DayCount_Max) { //Year Update, Day Reset
    DayCount -= DayCount_Max;
    YearCount += 1;

    //DataLog Values to the Year Based File
    //Average Min Values 

    //Save Data we Captured to File
    File datalogFile_Year = SD.open("year.txt", FILE_WRITE);
    if (datalogFile_Year) {
      datalogFile_Year.println(dataString);
      datalogFile_Year.close();
      if (Serial) Serial.println("Updated Year_Datalog.txt");
    }
    else if (Serial) Serial.println("Error Opening Year Datalog.txt");
  }
  if (YearCount >= YearCount_Max) { //Decade Update, Year Reset
    YearCount -= YearCount_Max;

    //DataLog Values to the Decade Based File
    //Average Min Values 

    //Save Data we Captured to File
    //YearCount >= (YearCount_Max - 1) //Year Tick 10y Meaning 1 Decade?
  }
}

void EndDelay(int x) {
  ///* //Disable // Enable time based Code
  for(int i = 0; i < x; i++) {
    digitalWrite(StatusLed, HIGH);
    delay(50);
    digitalWrite(StatusLed, LOW);
    delay(950);
  }
  //*/
}
