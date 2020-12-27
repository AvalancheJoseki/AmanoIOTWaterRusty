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

/* New node checklist
-LoRa

Primary detection method: ultrasonic
Secondary detection method: pressure
Overflow detection: contact

-Battery backup.
-Can detect battery voltage.

ESP powered (so that it can be updated wirelessly) // this isnt a good option due to battery limitations

*/

#include <SPI.h> // include libraries
#include <LoRa.h>
#include <Amano.h>
#include<NewPing.h>

NewPing sonar(32, 33); // (trig, echo)

byte nodeID = 8;
Amano amano(nodeID);  // (nodeID)
payload localStruct;

int ssPin = 5, rstPin = 17, dio0Pin = 16;

unsigned int msgCount = 0;      // count of outgoing messages

char waterChar = 'L'; 
unsigned int lastWaterLevel = 0, waterLevel = 0;
unsigned long lastContact = 0;

// water level pins
// int percent25pin = 4;
// int percent50pin = 5;
// int percent75pin = 6;
// int percent90pin = 7;

int voltagePin = 32;

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
  return (float)analog*(5.4599) - 31.026; // returns millivolts
} // end of getBatteryVoltage ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

/*int checkWaterLevel(void)
{
  // check water level then send command to the pump house every minutesUntilPing minutes
  int percent25Level = 0, percent50Level = 0, percent75Level = 0, percent90Level = 0, currentLevel =100;
  //Checks for 25%
  digitalWrite(percent25pin, HIGH);
  delay(20);
  percent25Level = analogRead(A0);
  if (percent25Level > 0)
  {
    currentLevel = map(percent25Level, 0, 314, 0, 50);
  }
  else
  {
    //Serial.println("SoMeThInG'S wRoNg");
    currentLevel = 0;
  }
  //Serial.print("Checking 25%");
  //Serial.println(analogRead(A0));
  digitalWrite(percent25pin, LOW);

  //Checks for 50%
  digitalWrite(percent50pin, HIGH);
  delay(20);
  percent50Level = analogRead(A0);
  if (percent50Level > 0)
  {
    currentLevel = map(percent50Level, 0, 173, 50, 75);
  }
  //Serial.print("Checking 50%");
  //Serial.println(analogRead(A0));
  digitalWrite(percent50pin, LOW);

  //Checks for 75%
  digitalWrite(percent75pin, HIGH);
  delay(20);
  percent75Level = analogRead(A0);
  if (percent75Level > 0)
  {
    currentLevel = map(percent75Level, 0, 142, 75, 90);
  }
  //Serial.print("Checking 75%");
  //Serial.println(analogRead(A0));
  digitalWrite(percent75pin, LOW);

  //Checks for 90%
  digitalWrite(percent90pin, HIGH);
  delay(20);
  percent90Level = analogRead(A0);
  if(percent90Level > 70)
    currentLevel = 100; // likely overflowing
  else if (percent90Level > 0)
  {
    currentLevel = map(percent90Level, 0, 70, 90, 100);
  }
  //Serial.print("Checking 90%");
  //Serial.println(analogRead(A0));
  digitalWrite(percent90pin, LOW);

  return currentLevel;
} //****************************************end of checkWaterLevel****************************************
*/

void setup()
{
  Serial.begin(9600); // initialize serial

  // pinMode(percent25pin, OUTPUT);
  // pinMode(percent50pin, OUTPUT);
  // pinMode(percent75pin, OUTPUT);
  // pinMode(percent90pin, OUTPUT);
  pinMode(ssPin, OUTPUT);
  pinMode(rstPin, OUTPUT);
  pinMode(dio0Pin, OUTPUT);
  pinMode(voltagePin, INPUT);

  if(!amano.begin(ssPin, rstPin, dio0Pin)){
      Serial.println("LoRa initialization FAILED");
      ;
  }
  else{
    Serial.println("rusty node LoRa connected.");
    ;
  }

} //****************************************end of setup****************************************


void loop()
{
  // broadcast water level every minute
   if (amano.itsMySecond()) {
      // waterLevel = checkWaterLevel();
      waterLevel = getBatteryVoltage(voltagePin);

      if(waterLevel < lastWaterLevel)
        waterChar = 'b';
      else if(waterLevel > lastWaterLevel)
        waterChar = 'B';

      lastWaterLevel = waterLevel;

    payload outgoingPayload = {nodeID/10, nodeID, waterChar, ++msgCount, waterLevel};
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

// checkWaterLevel();
} //****************************************end of loop****************************************