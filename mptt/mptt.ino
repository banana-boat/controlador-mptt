#include<Servo.h>
#include <SPI.h>
#include <SD.h>

Servo ESC; //Crear un objeto de clase servo

#define NUM_SAMPLES 100
#define CONTROL_ESC_VOLTS 10
#define MID_PWM 1400
#define MIN_PWM 1400
#define MAX_PWM 1400


//***********************Change between this lanes****************************
int midpwm = 1400;             //This is the velue where the converter stays at his output voltage
int maxpwm = 2000;             //Take a value higer than the midpwm but not to high
int minpwm = 1000;             //Take a value lower than the midpwm but not to low
//*****************************************************************************

const int chipSelect = 4;

//All data for the MPPT controler
int panelMeter = 0;
int panelAmpMeter = 1;
int escMeter = 2;
int escAmpMeter = 3;
int releControl = 8;
int escControl = 9;
float sensibilidad=0.1;     //sensibilidad del sensor acs712

//All of the calculated data
float panelVolts;
float panelVoltsDelta;
float panelVoltsPrev;
float escVolts;
float escVoltsDelta;
float escVoltsPrev;
float watt;
float wattDelta;
float panelAmp;
float panelAmpin;
float panelAmpDelta;
float panelAmpPrev;
float escAmpDelta;
float escAmp;
float escAmpin;
float escAmpPrev;



//Just some other data
int   stepAmount;
int   lastStepAmount;
int   pwm = 1000;
int   peakPwm = 1000;
float peakWatt = 0;
int   counter;
boolean escDown = true;
boolean engineDown = true;
boolean SDCardAvailable =true;

byte PWM_PIN = 3;
int  pwm_value=0;
int gradient = 0;

//estados: "searching", "adjusting" "stable"
String mpttStatus = "searching";

int l = -1;
int r = -1;
int m1 = -1;
int m2 = -1;
int m1_result = -1;
int m2_result = -1;
int prev_m1_result = -1;
int prev_m2_result = -1;

void init_esc() {
  //rele abierto
  digitalWrite(releControl, HIGH);

  //paso1 armado de variador
  ESC.writeMicroseconds(2000);
  delay(500);

  //rele cerrado
  digitalWrite(releControl, LOW);
  delay(1500);

  //paso2 armado de esc
  ESC.writeMicroseconds(1000);
  escDown = false;

  //TODO revisar este valor
  engineDown = false;
  delay(1500);

  //incializamos algoritmo
  stepAmount = -1;
  
}

void run_powerControl() {  //Function for controlling panelVolts
  lastStepAmount = stepAmount;
  stepAmount = 0;
  
  if (mpttStatus == "searching") {
    if (CONTROL_ESC_VOLTS<=escVolts) {
      // TODO cambiar lastStepAmount de valor neg a flag
      if (lastStepAmount == -1) {
        stepAmount = 20;
      } else if (wattDelta>2.0) {
        stepAmount = 20;
      } else if (wattDelta>0.0){
        stepAmount = 10;
      } else {
        //TODO revisar esta condicion
        mpttStatus = "adjusting";
        pwm = peakPwm -20;
      }
    } else {
      stepAmount = -20;
      mpttStatus = "adjusting";
    }
  } else if (mpttStatus == "adjusting"){
    if (CONTROL_ESC_VOLTS<=escVolts) {
      if (wattDelta>0.1) {
        stepAmount = 5;
      } else {
        pwm = peakPwm;
        mpttStatus = "stable";
      }
    } else {
      stepAmount = -10;
      //mpttStatus = "searching";
    }
  } else {

    // Si baja Amp. panel = > tengo menos radiacion
    // Si sube Amp. panel = > tenemos mas radiación
    if (wattDelta>2) {
      stepAmount = 20;
      mpttStatus = "searching";
      peakWatt = watt;
      peakPwm = pwm;
    } else if (wattDelta<=-2) {
      stepAmount = -30;
      mpttStatus = "searching";
      peakWatt = watt;
      peakPwm = pwm;
    }
  }
     
  // Calculo de pwm y watt maximo
  if (peakWatt < watt) {
    peakWatt = watt;
    peakPwm = pwm;
  }

  pwm = pwm + stepAmount;
  if (pwm <= 1100) pwm = 1120;                             //Check for underload data. If your convert goes lower than this value change this
  if (pwm > 2000) pwm = 2000;                           //Check for overload data. This is the maximum pwm data.
  ESC.writeMicroseconds(pwm);
}


void poweroff_esc() {
  ESC.writeMicroseconds(1000);
  //rele abierto
  digitalWrite(releControl, HIGH);
  escDown = true;
}

void poweroff_engine() {
  ESC.writeMicroseconds(1000);
  delay(500);
  engineDown = true;
}

void poweron_engine() {
  ESC.writeMicroseconds(peakPwm);
  pwm = peakPwm;
  delay(500);
  engineDown = false;
}

void read_data() {

  panelVoltsPrev = panelVolts;
  escVoltsPrev = escVolts;
  panelAmpPrev = panelAmp;
  escAmpPrev = escAmp;
  //wattDelta = watt;

  //********************Solar cell Voltage
  panelVolts = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    panelVolts += analogRead(panelMeter);
  }
  panelVolts = (panelVolts*0.0537)/NUM_SAMPLES;
  panelVoltsDelta = (panelVolts-panelVoltsPrev)+panelVoltsDelta;
  
  //********************esc Voltage
  escVolts = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    escVolts += analogRead(escMeter);
  }
  escVolts = (escVolts*0.0537)/NUM_SAMPLES;
  escVoltsDelta = (escVolts-escVoltsPrev)+escVoltsDelta;

  
  //********************Solar cell Current
  panelAmp = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    panelAmpin = analogRead(panelAmpMeter) * 5.0 / 1023.0;
    panelAmpin = (panelAmpin - 2.5) / sensibilidad;
    if (panelAmpin <= 0) panelAmpin = 0;
    panelAmp += panelAmpin;
  }
  panelAmp = panelAmp / NUM_SAMPLES;  
  panelAmpDelta = (panelAmp - panelAmpPrev)+panelAmpDelta;

  //********************Solar cell Current
  escAmp = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    escAmpin = analogRead(escAmpMeter) * 5.0 / 1023.0; 
    escAmpin = (escAmpin - 2.5) / sensibilidad;
    if (escAmpin <= 0) escAmpin = 0;                             
    escAmp += escAmpin;
  }
  escAmp = escAmp / NUM_SAMPLES;
  escAmpDelta = (escAmp - escAmpPrev)+escAmpDelta;

  //calculamos potencia
  wattDelta = ((panelAmp * panelVolts)-watt) + wattDelta;
  watt = panelAmp * panelVolts;
}

void print_sd(String line) {
  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  File dataFile = SD.open("banana.log", FILE_WRITE);

  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println(line);
    dataFile.close();
    // print to the serial port too:
  }
  // if the file isn't open, pop up an error:
  else {
    Serial.println("error opening datalog.txt");
  }
}

void print_data() {
  
  String dataString = "";
  dataString += "Vpanel: ";
  dataString += String(panelVolts);
  dataString += " Apanel: ";
  dataString += String(panelAmp);
  dataString += " Watt: ";
  dataString += String(watt);
  dataString += " WattDelta: ";
  dataString += String(wattDelta);
  dataString += " Vesc: ";
  dataString += String(escVolts);
  dataString += " Aesc: ";
  dataString += String(escAmp);
  dataString += " PWM: ";
  dataString += String(pwm);
  dataString += " Status: ";
  dataString += String(mpttStatus);
  dataString += " Step: ";
  dataString += String(stepAmount);

  Serial.println(dataString);
  if (SDCardAvailable)
    print_sd(dataString);
}


void setup() {
  // inicializamos monitor serie
  Serial.begin(9600);
  Serial.setTimeout(10);

  Serial.println("Initializing SD card...");

  // see if the card is present and can be initialized:
  SDCardAvailable = true;
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    SDCardAvailable = false;
  }
  Serial.println("card initialized.");
  
  //Asignar un pin al ESC
  ESC.attach(escControl);

  //asignamos pin de rele
  pinMode(releControl, OUTPUT);

  //Valor de la emisora
  pinMode(PWM_PIN, INPUT);


  read_data();
  delay(500);
  if (panelVolts > 5) {
    init_esc();
  }
  pwm = 1000;         		//motor apagado
  watt = 0;
  peakWatt = 0;
  peakPwm = 1000;
  panelVoltsDelta = 0;
  escVoltsDelta = 0;
  panelAmpDelta = 0;
  escAmpDelta = 0;
  wattDelta = 0;
  print_data();
}

//Repeat this continously
void loop() {
  boolean all_good = run_safety_controls();
  if(!all_good) return;

  search_value();
}

boolean run_safety_controls(){
  if (panelVolts <= 5) {               //When the output of the converter is totaly closed or opened
    //chequear si ESC está apagado
    if (!(escDown)) poweroff_esc();
    if (SDCardAvailable)
      print_sd("[Alert] panelVolt < 5V");
    delay(500);
    return false;
  }

  if (escDown) {
    init_esc();
  }

  pwm_value = pulseIn(PWM_PIN, HIGH);
  if (SDCardAvailable)
    print_sd("Throttle: " + String(pwm_value));
  if ((pwm_value<=1200)&&(pwm_value!=0)) {
    if (engineDown){
      poweron_engine();
    }
  } else {
    poweroff_engine();
    delay(100);
    return false;
  }
  //proteccion
  if (pwm >= maxpwm) {
    pwm = maxpwm;
  }
  else if (pwm<minpwm) {
    pwm = minpwm;
  }
}

void search_value(){
  counter++;
  if (counter < 5) {
    //print_data();
    return;
  }

  //calculate_new_values();
  if(l == -1) l = minpwm;
  if(r == -1) r = maxpwm;

  m1 = l + (r - l) / 3;
  m2 = r - (r + l) / 3;
  //send, wait
  pwm = m1;
  ESC.writeMicroseconds(pwm);
  delay(500);
  //get m1 result
  read_data();
  m1_result = watt;
  //send, wait
  pwm = m2;
  ESC.writeMicroseconds(pwm);
  delay(500);
  //get m2 result
  read_data();
  m2_result = watt;

  //Decide next interval
  if(m1_result < m2_result){
    l = m1;
  } else if(m1_result > m2_result){
    r = m2;
  } else {
    l = m1;
    r = m2;
  }

  //Reset if function changed
  if(m1_result < prev_m1_result && m1_result < prev_m2_result) {
    l = minpwm;
    r = maxpwm;
  }
  if(m2_result < prev_m1_result && m2_result < prev_m2_result){
    l = minpwm;
    r = maxpwm;
  }

  prev_m1_result = m1_result;
  prev_m2_result = m2_result;

  print_data();
  counter = 0;
  panelVoltsDelta = 0;
  escVoltsDelta = 0;
  panelAmpDelta = 0;
  escAmpDelta = 0;
  wattDelta = 0;
}

