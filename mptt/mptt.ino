#include<Servo.h>

Servo ESC; //Crear un objeto de clase servo

#define NUM_SAMPLES 100
#define CONTROL_ESC_VOLTS 10
#define MID_PWM 1400
#define MIN_PWM 1000
#define MAX_PWM 2000


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

// Contadores
int counter;
int rxCounter;

boolean escDown = true;
boolean engineDown = true;

byte PWM_PIN = 3;
int  pwm_value = 0;
int gradient = 0;

//estados: "searching", "adjusting" "stable"
String mpttStatus = "searching";

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
    }
  } else {

    // Si baja Amp. panel = > tengo menos radiacion
    // Si sube Amp. panel = > tenemos mas radiación
    if (wattDelta>2) {
      stepAmount = 10;
      mpttStatus = "searching";
      peakWatt = watt;
      peakPwm = pwm;
    } else if (wattDelta<=-2) {
      stepAmount = -20;
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
}


void setup() {
  // inicializamos monitor serie
  Serial.begin(9600);
  Serial.setTimeout(10);
  
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

  // inicializamos contadores
  counter = 0;
  rxCounter = 0;
  
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

void loop() {                 //Repeat this continously
  read_data();              //Get all data
  if (panelVolts <= 5) {               //When the output of the converter is totaly closed or opened
    //chequear si ESC está apagado
    if (!(escDown)) poweroff_esc();
    print_data();
    delay(500);
  } else {
    if (escDown) {
      init_esc();
    }
    pwm_value = pulseIn(PWM_PIN, HIGH);
      
    if ((pwm_value<=1300)&&(pwm_value!=0)) {
      if (engineDown){
        poweron_engine();  
      }
      
      //proteccion
      if (pwm >= MAX_PWM) {
        pwm = MAX_PWM;
      }
      if (pwm<MIN_PWM) {
        pwm = MIN_PWM;
      }
      if (counter >= 5) {
        //for changing the output.
        run_powerControl();
        print_data();
        counter = 0;
        panelVoltsDelta = 0;
        escVoltsDelta = 0;
        panelAmpDelta = 0;
        escAmpDelta = 0;
        wattDelta = 0;
      } else {
        print_data();
      }
      counter++;
    } else {
      if (!(engineDown) && (rxCounter >= 10)) {
        rxCounter = 0;
        poweroff_engine();
      }
    }
    rxCounter++;
    delay(100);
  }
}

