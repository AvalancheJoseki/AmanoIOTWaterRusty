/* ** on the idea of using a pressure sensor to detect water level **
 
The sensor itself must be in the "ballpark" of the desired pressure.
Since some pressure sensors are designed for closed systems, they are
way out of the range of our project. Here's the conversion:

1 meter of water = 9.80665 kPa
At amano, the tanks are roughly 30 meters above us. If we overshoot
to 50 meters, that means we need a sensor from 0-490kPa (0-71psi)

Pressure sensors come in two varieties. The one for closed (pressurized)
systems are referred to as absolute sensors.  If the top of the 
water tank (like ours) is vented, then our sensor can be a pressure
transducer. I think pressure gagues are a form of pressure transducer.
 
 depth(m) 	psi	      10 bit resolution(cm)   12 bit resolution (cm)
  7	        9.9561	  0.973225806	            0.243128205
  21	      29.8683	  2.919677419	            0.729384615
  42	      59.7366	  5.839354839	            1.458769231
  70	      99.561	  9.732258065	            2.431282051
 */

/* node information
use LOLIN D32 in arduino baords manager

a 500mAh battery will last roughly 18 hours

*/

#include <SPI.h> // include libraries
#include <LoRa.h>
#include <Amano.h>
#include<NewPing.h>


byte nodeID = 12; // rusty
// byte nodeID = 18; // freshy
Amano amano(nodeID);  // (nodeID)
payload localStruct;

int ssPin = 5, rstPin = 17, dio0Pin = 16;       // LoRa pins
int trigPin = 32, echoPin = 33;                 // distance pins
int voltagePin = 35;

unsigned int msgCount = 0;                      // count of outgoing messages

char waterChar = 'd'; 
unsigned int lastWaterLevel = 0, waterLevel = 0;
unsigned long lastContact = 0;

const byte pctFullArSize = 12;                   // for getting values throughout a minute
byte pctFullIndex = 0;
int pctFullAr[pctFullArSize] = {0};                       
bool havePolled = false;


int getMedian(int array[], int arSize){
  int top, last = arSize - 1,  ptr, ssf, temp;
    
  for (top = 0; top < last; top++){
    ssf = top;
    for (ptr = top; ptr <= last; ptr ++){
      if (array[ptr] < array[ssf]){  // > for descending
        ssf = ptr;
      }
    }
    temp = array[ssf];
    array[ssf] = array[top];
    array[top] = temp;
  }

  return array[arSize/2 + 1];
} // ************************************************************ end of getMedian ************************************************************

float getBatteryVoltage(byte pin){
  /* Voltage sensing methodology:
  3.3/1023 will give amount of voltage per adc step. If using a 470k/680k divider (see Tech Note 143 video) then the 
  Maximum voltage that can be read is 5.07v.  1023 should correspond to 5.07. That means each analog reading should
  be 5.07/1023 = 0.00495601173020527859237536656891 volts. 

  The above is theoretically accurate, but in practice inaccurate. There is input impedance on the GPIO pin itself
  that is variable from device and will affect the divider resistor values.  This can be corrected in software.
  Source of above info: https://youtu.be/5srvxIm1mcQ

  I ended up creating a regression equation from a few samples: y = 5.4599x - 31.026, where x=analogRead and y = mV

*/
    int analog = analogRead(pin);
    Serial.print("Analog: ");
    Serial.print(analog);
    Serial.print("\t");
  // return (float)analog*(5.4599) - 31.026; // returns millivolts
  return analog; // probably better just to use the adc value
} // end of getBatteryVoltage ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

int getDistanceMM(int degreesC = 24){ // (defaults to 24C)
  /* distance measuring methodology
  microseconds = pulseIn()
  seconds = microseconds/1000000

  speed of sound = 331m/s (but can vary depending on temperature)

  meters = 331m/s * microseconds/1000000
  millimeters = 331m/s * microseconds/1000000 * 1000
  millimeters = 331/1000*microseconds/2 (divide by 2 to get distance to but not from)
  millimeters = 331/2000*microseconds
  */
  int numSamples = 1;               // just get one sample (override from original method. now samples are spread throughout the minute)
  int pulseList[numSamples] = {0};
  unsigned int duration = 0;

  for(int i=0; i<numSamples; i++){  // get multiple readings and store them in pulseList[]
    digitalWrite(trigPin, LOW);
    delayMicroseconds(5);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    pulseList[i] = pulseIn(echoPin, HIGH); 
    if(numSamples > 1)
      delay(50);  // need a delay here, otherwise subsequent samples are inaccurate (lower)
  }
  
  if(numSamples > 1)
    duration = getMedian(pulseList, numSamples);
  else
    duration = pulseList[0];

  return (getSpeedOfSound(degreesC)/2000.0)*duration;   // convert to mm 
} //**************************************** end of getDistanceMM ****************************************

int getSpeedOfSound(int degreesC){
  return(331.3*sqrt(1 + degreesC/273.15)); // from Wikipedia
} //**************************************** end of getSpeedOfSound ****************************************

unsigned long combineWeatherInfo(int source, unsigned long temp, unsigned long pressure, unsigned long humidity){
  unsigned long combined = source*1000000000;
  combined+=100000*(temp);
  // combined+=10000*(pressure);
  combined+=1*(humidity);
  return combined;
} // *************************************************  end of combineWeatherInfo  *************************************************


int getPercentFull(void){
  /* water level methodology
  The water tank is (approx) 210cm tall.
  The water overflows at (approx) 72cm tall. 
  */
  int   distanceFromBottom = 2100, overflowHeight = 1720,
        distanceFromOverFlow = distanceFromBottom - overflowHeight,
        waterLevelMM = distanceFromBottom - getDistanceMM(),
        waterLevelPct = 100*100.0*waterLevelMM / overflowHeight;
  Serial.print("value: ");
  Serial.print(waterLevelPct);
  if(waterLevelPct%100 > 50){
    Serial.println("rounding up");
    return waterLevelPct/100 + 1;
  }
  else{
    Serial.println("rounding down");
    return waterLevelPct/100;
  }
} //****************************************end of getPercentFull****************************************

void setup(){
  Serial.begin(9600);

  pinMode(voltagePin, INPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  if(!amano.begin(ssPin, rstPin, dio0Pin)){
      Serial.println("LoRa initialization FAILED");
      ;
  }
  else{
    if(nodeID == 12)
      Serial.println("rusty node LoRa connected.");
    else 
      Serial.println("freshy node LoRa connected.");
    ;
  }

} //****************************************end of setup****************************************

void loop(){
   // broadcast water level every minute
   if (amano.itsMySecond()) {
      waterLevel = getMedian(pctFullAr, pctFullArSize);
      Serial.println("pct array:");
      for(int i=0; i<pctFullArSize; i++){
        Serial.print(" [");
        Serial.print(i);
        Serial.print("]:");
        Serial.println(pctFullAr[i]);
      }

      if(nodeID == 12)
        if(waterLevel < lastWaterLevel)
          waterChar = 'l';
        else if(waterLevel > lastWaterLevel)
          waterChar = 'L';
      else if(nodeID == 18)
        if(waterLevel < lastWaterLevel)
          waterChar = 'w';
        else if(waterLevel > lastWaterLevel)
          waterChar = 'W';

      lastWaterLevel = waterLevel;

    payload outgoingPayload = {nodeID/10, nodeID, waterChar, ++msgCount, combineWeatherInfo(1, getBatteryVoltage(voltagePin),0,waterLevel)};
    amano.sendMessage(&outgoingPayload);
    Serial.print((char)waterChar);
    Serial.print(" ");
    Serial.println(waterLevel);
  }

  // parse for a packet, and call onReceive with the result:
  if (amano.onReceive(&localStruct)){
      switch(localStruct.type){ 
          break;
        default:
          ;
      }
      Serial.println((char)localStruct.type);//+" "+localStruct.message);
  } 

  if(millis()%10000 < 5000){
    if(!havePolled){
      pctFullAr[pctFullIndex++] = getPercentFull();
      if(pctFullIndex == pctFullArSize)
        pctFullIndex = 0;
      havePolled = true;
    }
  }
  else
    havePolled = false;  
} //****************************************end of loop****************************************