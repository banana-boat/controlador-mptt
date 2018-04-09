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

//************************Change between this lanes****************************
int midpwm = 146;             //This is the velue where the converter stays at his output voltage
int maxpwm = 200;             //Take a value higer than the midpwm but not to high
int minpwm = 100;             //Take a value lower than the midpwm but not to low
int pixdivider = 4;           //This value wil
float pix_max_voltage = 3.3;  //This is the value for the maximum voltage of the pixhawk analog sensor

//*****************************************************************************

//All data for the MPPT controler
int panelMeter = 6;       //Raw data from the Voltage of the solar cells
int panelAmpMeter = 5;    //Raw data from the Current of the solar cells
int batteryMeter = 4;     //Raw data from the Voltage of the Battery
int driver = 6;           //pwm output data for the DC-DC converter

//Extra option for data logging on Pixhawk
int pixhawk = 11;         //output data for logging Pixhawk
int pixhawkCheck = 3;     //Measurment data for checking data. More detail later

//All of the calculated data
float panelVolts;
float batteryVolts;
float watt;
float panelAmp;
float panelAmpin;
float panelVoltsCompare;
float batteryVoltsCompare;
float wattCompare;
float panelAmpCompare;
float batteryAmp;

//Just some other data
int   mpptout = 255;                                //The MPPT value for raising or dropping the voltage
float controlPanelVolt;                           //The MPPT will change this value
float stepAmount;                                 //For changing the pwmput voltage take bigger stepps when further awy from target
float pwm = 150;                                  //The value that is send to the converter
float pixout;                                     // The value send to the pixhawk

int counter;                                      //Counter for the MPPT algorithme

void setup() {                                    //Do once
  Serial.begin(9600);

  pinMode(panelMeter, INPUT);
  pinMode(panelAmpMeter, INPUT);
  pinMode(batteryMeter, INPUT);
  pinMode(driver, OUTPUT);
	pinMode(pixhawk, OUTPUT);

  delay(500);
  read_data();                                    //Get all data

  //We will start first at checking what the best MPP is
  controlPanelVolt = panelVolts /100 * 76;        //Estimated value for quick mppt control 76% of the panelVolts
  mpptout = controlPanelVolt * 11.086956;         //Write this data to solarConverter

  print_data();                                   //Write this data on the serial monitor
}

void loop() {                 //Repeat this continously
    read_data();              //Get all data

    while(panelVolts <=5){      //Change nothing when solar voltage <= 5, because it is to low for the converter
      read_data();
      print_data();
	    log_pixhawk();
	    pwm = maxpwm;                //Write this value so the converter isn't giving any current
		  analogWrite(driver, pwm); //Do above
		}

    if(counter >= 5){           //Wait some loops before making a choice for the MPPT Algorithme so the converter get some time
      //for changing the output.
			run_mppt();               //Do mppt loop for mppt solar
			counter = 0;              //Reset counter
	  }
		counter++;                  //Add the counter

    /* Sometimes the code is stuck at a to low voltage we need a small reset
     * This happens when there is a sudden big change in the amount of sun
     * Just by closing the output and calculating the estimate value (76% of the voltage) 
     */
	  if(pwm == maxpwm or pwm == minpwm){                 //When the output of the converter is totaly closed or opened
	    if(pwm == minpwm){                             // for an open output, first close it and wait a moment.
	      pwm = maxpwm + 1;                            //Let the output current stop. The +1 is that you know it is in this if state
		    analogWrite(driver, pwm);
			  delay(500);
				pwm = midpwm;                                //This is the velue where the converter stays at his output voltage
		  }
			Serial.println();                           //Give an extra line, so we can see that this happens
			read_data();                                //Get all data
			controlPanelVolt = panelVolts /100 * 76;    //We know that when there is no current the mpp is 76% of the voltage
			mpptout = (controlPanelVolt - 9) * 11.086956;   //Write this data to solarConverter
		}

  run_voltageControl();      //Change pwmvoltage to right mppt
    print_data();
      log_pixhawk();              //Give pwm Amp information to Pixhawk 1V = 4A
      }

void read_data() {            //Function for reading analog inputs
 /*
   * What I am doing here is reading all the data and calculate the avarage of 100
     * Before doing that I need to remember the old data.
       */

  panelVoltsCompare = panelVolts;
    batteryVoltsCompare = batteryVolts;
      panelAmpCompare = panelAmp;
        wattCompare = watt;

  //********************Solar cell Voltage
    panelVolts = 0;
      for(int i=0;i<100;i++){
          panelVolts = panelVolts + (analogRead(panelMeter) / 29.602888);             // read the panel voltage 100 times and add the values together
	    }
	      panelVolts = panelVolts/100;
	        //********************Battery Voltage
		  batteryVolts = 0;
		    for(int i=0;i<100;i++){
		        batteryVolts = batteryVolts + (analogRead(batteryMeter) / 74.93796526);    // read the battery voltage 100 times and add the values together
			  }
			    batteryVolts = batteryVolts/100;
			      //********************Solar cell Current
			        panelAmp = 0;
				  for(int i=0;i<100;i++){
				      panelAmpin = analogRead(panelAmpMeter)*5.0/1023.0;                          // reads the panelAmpere 100 times and add the values together
				          panelAmpin = (panelAmpin-2.5)*10;
					      if(panelAmpin <= 0) panelAmpin = 0;                                         // MPPT algorithm does not work with negative values
					          panelAmp += panelAmpin;
						    }
						      panelAmp = panelAmp/100;

  watt = panelAmp * panelVolts;
    batteryAmp = watt / batteryVolts - 1.3;               //We need this for logging to the Pixhawk

  //Calculate the Delta of everythig for the MPPT algorithm
    panelVoltsCompare = panelVolts - panelVoltsCompare;
      batteryVoltsCompare = batteryVolts - batteryVoltsCompare;
        panelAmpCompare = panelAmp - panelAmpCompare;
	  wattCompare = watt - wattCompare;
	  }

void run_mppt() {             //Function for mpp tracker. Here you can change the algorithme.
  if(wattCompare > 0){
      if(panelVoltsCompare > 0){
            mpptout++;
	        }
		    else{
		          mpptout--;
			      }
			        }
				  else{
				      if(panelVoltsCompare > 0){
				            mpptout--;
					        }
						    else{
						          mpptout++;
							      }
							        }


  //The maximum and minimum values.
    if(mpptout > 255 )mpptout = 255;    //Check for overload data
      if(mpptout < 0) mpptout = 0;        //Check for underload data
      }

void run_voltageControl() {  //Function for controlling panelVolts
  controlPanelVolt = mpptout / 11.086956;             //Calculate data to voltage 0 - 23
    controlPanelVolt += 9;                              //Ofset for right voltage 9 - 32 we can not charge when battery is under 9

  stepAmount = (controlPanelVolt - panelVolts)/2;     //Calculate + or - and for higer difference take bigger steps

  if(stepAmount < 0) stepAmount = -1;
    if(stepAmount > 0) stepAmount = 1;

  if(batteryVolts >= 13.0){                           //Check if the battery is full when using other kind of battery change this
      pwm += 5;                               //When the battery is full, the only thing we have to do is to lower the current by closing the gate
        }
	  else{
	      pwm += stepAmount;                                //Add the data for the pwmmpptoutput, when battery is not ful
	        }

  if(pwm < 50) pwm = 50;                              //Check for underload data. If your convert goes lower than this value change this
    if(pwm > 255) pwm = 255;                            //Check for overload data. This is the maximum pwm data.
      analogWrite(driver, pwm);                           //Write data to the converter

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
  Serial.print("Vbatt:");     //Voltage of the battery
  Serial.print(batteryVolts);
  Serial.print("\t");
  Serial.print("Abatt:");     //Calculated current of the battery
  Serial.print(batteryAmp);   
  Serial.print("\t");
  Serial.print("MPPTO:");     //Thempptoutput of the mppt controler
  Serial.print(mpptout);
  Serial.print("\t");
  Serial.print("CPV:");       //The calculated voltage of the solar cells mpptout of the mppt
  Serial.print(controlPanelVolt);
  Serial.print("\t");
  Serial.print("outpwm:");    //The actualmpptoutput from the converter
  Serial.print(pwm);
  Serial.print("   \t");
  Serial.print("StA:");       //The step amount
  Serial.print(stepAmount);
  Serial.println();
}

void log_pixhawk() {          //Function for Pixhawk logging
  /*
   * Because the mpptoutput voltage with the PWM value are not linear.
   * We need to constantly measure the out value of the arduino.
   * This way we can exactly log the data on the Pixhawk
   */
//  batteryAmp = 6;                               //You can use this when the solar cells are not connected and you want to test 
  float value = batteryAmp / pixdivider;                     //Change value to pwmoutput voltage
  float check = analogRead(pixhawkCheck)/204.6;   //Get data from last writen data

  float stepAmountPix = (value - check)/2;        //Calculate the stepAmount for making bigger steps
  pixout = pixout + stepAmountPix;
  
 
  if(pixout > (pix_max_voltage * 45)) pixout = (pix_max_voltage * 45); //Check if it is not too high for the Pixhawk protection so the voltage 
                                                                              //can't go higher and you will not damage the pixhawk
  if(pixout < 0) pixout = 0;                      //Check if it is not lower than 0
  analogWrite(pixhawk, pixout);                   //Write data
}
