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
January 2nd 2016   : Storage Update for MTC, Now MultiStores S,M,H,D and Y and Linerization Added to Calculations

January 3rd 2016   : Storage Array for Average Values
                   
-------------------------------------------------------------------------------------------------------*/

//SD Card Setup and File Delimiters
#include <SPI.h>
#include <SD.h>
  const int chipSelect = 4;
  String dataString = "";
  String Split = "/";
  String End = ".";

//Pin Setup and Calculation Data
#define StatusLed 13
#define VBatLipo A9
#define VBatLeadAcid A2
#define VSolar A0
#define ISolar A1
  float MeasuredVBatLipo;
  float MeasuredVBatLeadAcid;
  float MeasureVSolarPanel;
  float MeasureISolarPanel;
  float MeasureWSolarPanel;

  float UnderVBatLipo = 3.300;               //Set Under Voltage Lipo
  float UnderVBatLeadAcid = 10.500;          //Set Under Voltage Lead Acid

  bool LipoUnderChargedVoltage = false;      //
  bool LeadAcidUnderChargedVoltage = false;  //
  bool PanalUnderChargeVoltage = false;      //Cloudy or Night Time?
  bool PanalAtChargeVoltage = false;         //Charging the Battery
  bool PanalOverChargeVoltage = false;       //Sunny But Don't Need to Charge

  bool Debug = true;
  int VoltageOffset = 1;

//This is the "MTC ( MicroProcessor Time Clock )" Setup
  int CalcTimeOffset = 59; //mS, +50mS for LED Flash, The Offset to Correct for a Second of Time

  int SecondCount_Max = 60;  //60 Sec per Min
  int MinuteCount_Max = 60; //60 Min per Hour
  int HourCount_Max = 24;    //24 Hours per Day
  int DayCount_Max = 365;    //365 Days per Year
  int YearCount_Max = 10;    //10 Years worth of Logs....
  
  int SecondCount = -1;      //-1 --> Init Print the Data Lines Names to .TXT
  int MinuteCount = 0;
  int HourCount = 0;
  int DayCount = 0;
  int YearCount = 0;

//Storage Array to Create the Average Values From
  const int dataspots = 4;
/*
int CapturesperMinuite[MinuiteCount_Max,dataspots];
  int CapturesperHour[HourCount_Max,dataspots];
  int CapturesperDay[DayCount_Max,dataspots]; 
  int CapturesperYear[YearCount_Max,dataspots];

int[4] MinuiteAverage;
  int[4] HourAverage;
  int[4] DayAverage;
  int[4] CapturesperYear;
*/

//Peek Values ( Dunno Yet )


//---------------------------------------------------------------------------------------------------------------------------------------

void setup() {
  pinMode(StatusLed, OUTPUT);
  Serial.begin(9600);

  digitalWrite(StatusLed, HIGH);
  delay(5000); //while (!Serial) { }
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
  delay(50);
  
  //Grab Values from Analog Input
  MeasuredVBatLipo = analogRead(VBatLipo);
  MeasuredVBatLeadAcid = analogRead(VBatLeadAcid);
  MeasureVSolarPanel = analogRead(VSolar);
  MeasureISolarPanel = analogRead(ISolar);

  
  //Calculate to Correct Values
  //Internal Resistor Divider Setup 2x
  MeasuredVBatLipo *= 2; //Resistor Divider Inside is 2x
  MeasuredVBatLipo *= 3.3; //WORKS Convert to Voltage of Board
  MeasuredVBatLipo /= 1024; //WORKS Convert the Counts per Voltage
  MeasuredVBatLipo *= 1.01; //% Error Offset


  //External Resistor Divider Setup 4x
  MeasuredVBatLeadAcid *= 4; //Resistor Divider Inside is 4x
  MeasuredVBatLeadAcid *= 3.3; //WORKS Convert to Voltage of Board
  MeasuredVBatLeadAcid /= 1024; //WORKS Convert the Counts per Voltage
  
  //Correction
  //Measured > Readout
  //10.64v --> 10.65v = % Error @ 4x
  //11.81v --> 11.83v = % Error @ 4x
  //12.49v --> 12.53v = % Error @ 4x
  //13.16v --> 13.06v = % Error @ 4x
  //13.55v --> 13.06v = % Error @ 4x
  //14.49v --> 6.v = % Error @ 4x
  //MeasuredVBatLeadAcid *= 1; //
  //MeasuredVBatLeadAcid += 0; //
  MeasuredVBatLeadAcid *= 0.99; //% Error Offset

  //External Autopilot 50V 180A Version
  //50V/180A = 63.69mV / Volt, 18.30mV / Amp.
  //3.3v / 1024 = 3.22mV per Step


  //Solar Panel Voltage ( With Lineraization )
  //63.69mV / Volt / 3.22 = 19.7795
  MeasureVSolarPanel *= 15.8; //Set for Somewhat Real Data was 0.0754 in Wattmeter 
  MeasureVSolarPanel *= 3.3; //WORKS Convert to Voltage of Board 
  MeasureVSolarPanel /= 1024; //WORKS Convert the Counts per Voltage
  
  //Correction
  //Measured > Readout
  //12.37v --> 12.9v = 3% Error @ 15.8x
  //12.42v --> 12.64v = % Error @ 15.8x
  //MeasureVSolarPanel *= 1;    //
  //MeasureVSolarPanel += 0;    //
  MeasureVSolarPanel *= 1.03; //% Error Offset


  //Solar Panel Current ( With Lineraization )
  //18.30mV / Amp / 3.22 = 5.6832
  MeasureISolarPanel *= 5; //Set for RAW Data was 0.67 in Wattmeter
  MeasureISolarPanel *= 3.3; //WORKS Convert to Voltage of Board
  MeasureISolarPanel /= 1024; //WORKS Convert the Counts per Voltage
  
  //Correction
  //Measured > Readout
  //
  //MeasureISolarPanel *= 1; //Set for RAW Data was 0.67 in Wattmeter
  //MeasureISolarPanel += 0; //
  //MeasureISolarPanel *= 1; //% Error Offset


  //Panel Wattage Calulation ( Simple )
  MeasureWSolarPanel = MeasureVSolarPanel * MeasureISolarPanel;


  //Logic Statements ---------------------------------------------------------------------------------------------------------------------

  if (MeasuredVBatLipo <= UnderVBatLipo) LipoUnderChargedVoltage = true;
  else LipoUnderChargedVoltage = false;

  if (MeasuredVBatLeadAcid <= UnderVBatLeadAcid) LeadAcidUnderChargedVoltage = true;
  else {
    LeadAcidUnderChargedVoltage = false;
    
    if (MeasureVSolarPanel < (MeasuredVBatLeadAcid - VoltageOffset)) {
      PanalUnderChargeVoltage = true;
      PanalAtChargeVoltage = false;
      PanalOverChargeVoltage = false;
    }
    else if(MeasureVSolarPanel >= (MeasuredVBatLeadAcid - VoltageOffset) && MeasureVSolarPanel <= (MeasuredVBatLeadAcid + VoltageOffset)) {
      PanalUnderChargeVoltage = false;
      PanalAtChargeVoltage = true;
      PanalOverChargeVoltage = false;
    }
    else if (MeasureVSolarPanel > (MeasuredVBatLeadAcid - VoltageOffset)) {
      PanalUnderChargeVoltage = false;
      PanalAtChargeVoltage = false;
      PanalOverChargeVoltage = true;
    }
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
      else Serial.println("LeadAcid Backup is Ready");
    Serial.println("-------------------------------");
    Serial.print("Solar Panel Voltage : " );
    Serial.println(MeasureVSolarPanel);
    Serial.print("Solar Panel Current : " );
    Serial.println(MeasureISolarPanel);
    Serial.print("Solar Panel Wattage : " );
    Serial.println(MeasureWSolarPanel);
      if (!PanalUnderChargeVoltage && !PanalAtChargeVoltage && !PanalOverChargeVoltage) Serial.println("No Solar Panel Found");
      else if(PanalUnderChargeVoltage) Serial.println("Not Enough Voltage to Charge");
      else if(PanalAtChargeVoltage) Serial.println("Charging Backup Battery");
      else if(PanalOverChargeVoltage) Serial.println("Float / Done Charging / Backup Battery");
      else Serial.println("Not Charging");
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
    //     SolarPanel_I,                        SolarPanel_W,                        Temperature,        Humidity.
    String(MeasureISolarPanel) + Split + String(MeasureWSolarPanel) + Split + String(0) + Split + String(0) + End;
  }

  //Datalog the Data that I Captured each Second;
  if (SecondCount < SecondCount_Max) {// && PanalAtChargeVoltage && ) { //Second Tick Capture
    File datalogFile_Sec = SD.open("sec.txt", FILE_WRITE);
      if (datalogFile_Sec) {
        datalogFile_Sec.println(dataString);
        datalogFile_Sec.close();
        if (Serial) Serial.println("Updated Second_Datalog.txt");
        if (Serial && Debug) Serial.println(dataString);
      }
      else if (Serial) Serial.println("Error Opening Second_Datalog.txt");
    //Save to the Array for Averaging Later

    //CapturesperMinuite[SecondCount, Dataspot] = 0;
  }

  if (SecondCount >= SecondCount_Max) { //Second Tick 60s Meaning 1 Min Capture
    File datalogFile_Min = SD.open("min.txt", FILE_WRITE);
      if (datalogFile_Min) {
        datalogFile_Min.println(dataString);
        datalogFile_Min.close();
        if (Serial) Serial.println("Updated Minuite_Datalog.txt");
      }
      else if (Serial) Serial.println("Error Opening Minuite Datalog.txt");
      //Save to the Array for Averaging Later

      //CapturesperHour[SecondCount, Dataspot] = 0;
  }

  if (MinuteCount >= MinuteCount_Max) { //Min Tick 60m Meaning 1 Hour Capture
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

  if (HourCount >= HourCount_Max) { //Hour Tick 24h Meaning 1 Day Capture
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
  
  if (DayCount >= DayCount_Max) { //Day Tick 365d Meaning 1 Year Capture
  File datalogFile_Year = SD.open("year.txt", FILE_WRITE);
    if (datalogFile_Year) {
      datalogFile_Year.println(dataString);
      datalogFile_Year.close();
      if (Serial) Serial.println("Updated Year_Datalog.txt");
    }
    else if (Serial) Serial.println("Error Opening Year Datalog.txt");
  }

  //YearCount >= YearCount_Max //Year Tick 10y Meaning 1 Decade?
  
  //End Data Poll ------------------------------------------------------------------------------------------
  digitalWrite(StatusLed, LOW);
  
  if (PanalAtChargeVoltage && !LipoUnderChargedVoltage) {
    delay(1000 - CalcTimeOffset); //Should be Charging, Watch this Data Closely
    SecondCount += 1;
  }
  else if (PanalOverChargeVoltage && !LipoUnderChargedVoltage) {
    delay(5000 - CalcTimeOffset); //Whats the Sun Levels Like?
    SecondCount += 5;
  }
  else if (PanalUnderChargeVoltage && !LipoUnderChargedVoltage) {
    delay(15000 - CalcTimeOffset); //Still want to know what the sun levels are like when low
    SecondCount += 14;
  }
  else if (!PanalUnderChargeVoltage && !PanalAtChargeVoltage && !PanalOverChargeVoltage && !LipoUnderChargedVoltage) {
    delay(30000 - CalcTimeOffset); //5min, 1s Datalog Feed if not Charging and Above 0V ( Cloudy / Night Mode )
    SecondCount += 30;
  }
  else {
    delay(60000 - CalcTimeOffset);  //I Dunno what the stare of the System is...
    SecondCount += SecondCount_Max;
  }


  
  //Set Some Loosed Based MTC Time for Graphing / Keeping Time
  if (SecondCount >= SecondCount_Max) {
    SecondCount -= SecondCount_Max;
    MinuteCount += 1;
  }
  if (MinuteCount >= MinuteCount_Max) {
    MinuteCount -= MinuteCount_Max;
    HourCount += 1;
  }
  if (HourCount >= HourCount_Max) {
    HourCount -= HourCount_Max;
    DayCount += 1;
  }
  if (DayCount >= DayCount_Max) {
    DayCount -= DayCount_Max;
    YearCount += 1;
  }
  if (YearCount >= YearCount_Max) {
    YearCount -= YearCount_Max;
  }
}
