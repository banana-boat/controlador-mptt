/*
  MPPT Design by FPVDrone Youtube link: https://www.youtube.com/channel/UCpubAmJoB5d_IUMVJCx0BQQ
  For more information about the converter check this:https://youtu.be/eTj1YSLMKSA
  For more information about the code check this:https://www.youtube.com/watch?v=x-tELQvq0Rk

  A MPPT algorithm is used for solar cells to get the highest power from the cells.
  It will continuously check if the power is more or less by changing the voltage of the solar cells.
  If the power is more than the previous we wil try to go furter other wise we will go back.
  This needs to be done as often as we can.

  There are 2 important loops:
  The first one is the MPPT algorithm.
   This one changes a value that later is calculated to the solar cells voltage input.

  At the second loop we have a loop that changes the current that flows through the converter.
  When the arduino sends a higher signal to the converter, it will decrease the ouput current.
   When the value is furter away from the midpoint it will decrease or increase faster.
    So what the loop will do is getting the MPPT threshold point and the solar cells voltage.
     When the voltage increases or decreses away from the threshold point it will change the current.

  Total loop example:
    1  The MPPT value goes up
      A  It means the solar cells increase (The threshold value)
    2  The converter value's are not the same
      A  Were we know to get a higher voltage we have to drop the current
	  3  The output value to the converter raise up so the current drops
*/

#include<Servo.h>
 
Servo ESC; //Crear un objeto de clase servo

#define NUM_SAMPLES 200

//************************Change between this lanes****************************
int midpwm = 1400;             //This is the velue where the converter stays at his output voltage
int maxpwm = 2000;             //Take a value higer than the midpwm but not to high
int minpwm = 1000;             //Take a value lower than the midpwm but not to low
//*****************************************************************************

//All data for the MPPT controler
int panelMeter = 0;        //PIN A0 Raw data from the Voltage of the solar cells
int panelAmpMeter = 1;     //PIN A1 Raw data from the Current of the solar cells
int escMeter = 2;          //PIN A2 Raw data from the Voltage of the ESC
int escAmpMeter = 3;     //PIN A3 Raw data from the Current of the ESC
int releControl = 8;        //PIN D8 para control de rele encendido del panel
int escControl = 9;         //PIN D9 ESC control
float sensibilidad=0.1;     //sensibilidad del sensor acs712

//All of the calculated data
float panelVolts;
float escVolts;
float watt;
float panelAmp;
float panelAmpin;
float escAmpin;
float panelVoltsCompare;
float escVoltsCompare;
float wattCompare;
float panelAmpCompare;
float escAmpCompare;
float escAmp;

boolean escDown = true;

//Just some other data
float controlPanelVolt;                           //The MPPT will change this value
float stepAmount;                                 //For changing the pwmput voltage take bigger stepps when further awy from target
int   pwm = 1000;                                  //The value that is send to the converter
int   peakPwm = 1000;
float peakWatt = 0;
int   counter;                                      //Counter for the MPPT algorithme

void setup() {
  // inicializamos monitor serie
  Serial.begin(9600);
  Serial.setTimeout(10);
  
  //asignamos pines medicion potencia
  //pinMode(panelMeter, INPUT);
  //pinMode(panelAmpMeter, INPUT);
  //pinMode(escMeter, INPUT);
  //pinMode(escAmpMeter, INPUT);
  //Asignar un pin al ESC
  ESC.attach(escControl);

  //asignamos pin de rele
  pinMode(releControl, OUTPUT);

  init_esc();

  delay(500);
  read_data();

  // inicializando valores
  controlPanelVolt = 11;	//3s
  pwm = 1000;         		//motor apagado
  watt = 0;
  peakWatt = 0;
  peakPwm = 0;

  print_data();
}

void loop() {                 //Repeat this continously
  read_data();              //Get all data
  if (panelVolts <= 5) {               //When the output of the converter is totaly closed or opened
    //chequear si ESC está apagado
    if (!(escDown)) poweroff_esc();
    print_data();
    delay(500);
    continue;
  }

  if (escDown) init_esc();

  //proteccion
  if ((pwm >= maxpwm or (pwm<1000)) {
     pwm = 1000;
  }

  //chequeamos si ESC esta apagado

  if (counter >= 5) {
    //for changing the output.
    run_powerControl();
    counter = 0;
  }
  counter++;                  //Add the counter
  print_data();
}


void run_powerControl() {  //Function for controlling panelVolts

     if (wattCompare>0){
     	if (wattCompare>2){
	   if (controlPanelVolt<=escVolts)){
	      stepAmount = 20;
	   } else {
	     stepAmount = -5;
	   }
	} else {
	  if (controlPanelVolt<=escVolts)){
	      stepAmount = 5;
	   } else {
	     stepAmount = -5;
	   }
	}
      else {
       if (abs(wattCompare)>2) {
       	  stepAmount = -20;
       } else {
       	 stepAmount = -5;
       }
     }

     // Punto estable???
     //if ((abs(wattCompare)<=1) && (counter > 1)) stepAmount = 0;
     
     // Calculo de pwm y watt maximo
     if (peakWatt < watt) {
     	peakWatt = watt;
	peakPwm = pwm;
     }

     pwm = pwm + StepAmount;
     if (pwm < 1100) pwm = 1100;                             //Check for underload data. If your convert goes lower than this value change this
     if (pwm > 2000) pwm = 2000;                           //Check for overload data. This is the maximum pwm data.
     ESC.writeMicroseconds(pwm);
     delay(40);
}

void poweroff_esc() {

  ESC.writeMicroseconds(1000);
  //rele abierto
  digitalWrite(releControl, HIGH);
  escDown = true;
}

void init_esc() {
  //rele abierto
  digitalWrite(releControl, HIGH);

  //paso1 armado de variador
  ESC.writeMicroseconds(2000);
  delay(500);

  //rele cerrado
  digitalWrite(releControl, LOW);
  delay(5000);

  //paso2 armado de esc
  ESC.writeMicroseconds(1000);
  escDown = false;
}

void read_data() {            //Function for reading analog inputs
  /*
      What I am doing here is reading all the data and calculate the avarage of 100
        Before doing that I need to remember the old data.
  */

  panelVoltsCompare = panelVolts;
  escVoltsCompare = escVolts;
  panelAmpCompare = panelAmp;
  escAmpCompare = escAmp;
  wattCompare = watt;

  //********************Solar cell Voltage
  panelVolts = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    panelVolts += analogRead(panelMeter);
  }
  panelVolts = (panelVolts*0.0537)/NUM_SAMPLES;

  
  //********************esc Voltage
  escVolts = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    escVolts += analogRead(escMeter);
  }
  escVolts = (escVolts*0.0537)/NUM_SAMPLES;
  
  //********************Solar cell Current
  panelAmp = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    panelAmpin = analogRead(panelAmpMeter) * 5.0 / 1023.0;
    panelAmpin = (panelAmpin - 2.5) / sensibilidad;
    if (panelAmpin <= 0) panelAmpin = 0;
    panelAmp += panelAmpin;
  }
  panelAmp = panelAmp / NUM_SAMPLES;

  //********************Solar cell Current
  escAmp = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    escAmpin = analogRead(escAmpMeter) * 5.0 / 1023.0;                      // reads the panelAmpere 100 times and add the values together
    escAmpin = (escAmpin - 2.5) / sensibilidad;
    if (escAmpin <= 0) escAmpin = 0;                                        // MPPT algorithm does not work with negative values
    escAmp += escAmpin;
  }
  escAmp = escAmp / NUM_SAMPLES;

  //calculamos potencia
  watt = panelAmp * panelVolts;  
  

  //Calculate the Delta of everythig for the MPPT algorithm
  panelVoltsCompare = panelVolts - panelVoltsCompare;
  escVoltsCompare = escVolts - escVoltsCompare;
  panelAmpCompare = panelAmp - panelAmpCompare;
  wattCompare = watt - wattCompare;
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
  Serial.print("StA:");       //The step amount
  Serial.print(stepAmount);
  Serial.println();
}

