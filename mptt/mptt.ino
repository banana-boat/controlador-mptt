#include<Servo.h>
 
Servo ESC; //Crear un objeto de clase servo

#define NUM_SAMPLES 200

//************************Change between this lanes****************************
int midpwm = 1400;             //This is the velue where the converter stays at his output voltage
int maxpwm = 2000;             //Take a value higer than the midpwm but not to high
int minpwm = 1000;             //Take a value lower than the midpwm but not to low
//*****************************************************************************

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

boolean escDown = true;

//Just some other data
float controlPanelVolt;                        
float stepAmount;
int   pwm = 1000;
int   peakPwm = 1000;
int   mptt = 0;
float peakWatt = 0;
int   counter;

void init_esc() {
  //rele abierto
  digitalWrite(releControl, HIGH);

  //paso1 armado de variador
  ESC.writeMicroseconds(2000);
  delay(500);

  //rele cerrado
  digitalWrite(releControl, LOW);
  delay(1000);

  //paso2 armado de esc
  ESC.writeMicroseconds(1000);
  escDown = false;
  delay(1000);
}

void run_powerControl() {  //Function for controlling panelVolts
  if (wattDelta>0.4) { 
    if ((stepAmount>0) && (controlPanelVolt<=escVolts)){
        stepAmount = 10;
    } else if ((stepAmount<0) || (controlPanelVolt<=escVolts)){
        stepAmount = -5;
    }
  } else if (abs(wattDelta)>0.4){
      if (stepAmount>0){
        stepAmount = -5;
      } else {
        stepAmount = 10;
      }
  } else {
    // No se que poner
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

void print_data() {           //Print all the information to the serial port
  Serial.print("Vpanel:");    //Voltage of the Solar cell
  Serial.print(panelVolts);
  Serial.print("\t");
  Serial.print("Apanel:");    //Current of the Solar cell
  Serial.print(panelAmp);
  Serial.print("\t");
  Serial.print("Watt:");      //Watt calculated
  Serial.print(watt);
  Serial.print("\t");
  Serial.print("Vbatt:");     //Voltage of the esc
  Serial.print(escVolts);
  Serial.print("\t");
  Serial.print("Abatt:");     //Calculated current of the esc
  Serial.print(escAmp);
  Serial.print("\t");
  Serial.print("outpwm:");    //The actualmpptoutput from the converter
  Serial.print(pwm);
  Serial.print("   \t");
  Serial.print("MPTT:");
  Serial.print(mptt);
  Serial.print("   \t");
  Serial.print("Count:");
  Serial.print(counter);
  Serial.print("   \t");
  Serial.print("StA:");       //The step amount
  Serial.print(stepAmount);
  Serial.println();
}


void setup() {
  // inicializamos monitor serie
  Serial.begin(9600);
  Serial.setTimeout(10);
  
  //Asignar un pin al ESC
  ESC.attach(escControl);

  //asignamos pin de rele
  pinMode(releControl, OUTPUT);

  read_data();
  delay(500);
  if (panelVolts <= 5) {
    init_esc();
  }
  // inicializando valores
  controlPanelVolt = 10;	//3s
  pwm = 1000;         		//motor apagado
  watt = 0;
  peakWatt = 0;
  peakPwm = 0;
  mptt = 0;

  print_data();
}

void loop() {                 //Repeat this continously
  read_data();              //Get all data
  if (panelVolts <= 5) {               //When the output of the converter is totaly closed or opened
    //chequear si ESC está apagado
    if (!(escDown)) poweroff_esc();
    delay(500);
  } else {
    if (escDown) {
      init_esc();
    }

    //proteccion
    if ((pwm >= maxpwm) or (pwm<minpwm)) {
      pwm = minpwm;
    }
    
    if (counter >= 5) {
      //for changing the output.
      run_powerControl();
      print_data();
      counter = 0;
      mptt = 0;
    } else {
      //print_data();
    }
    counter++;
    delay(100);
  }
}

