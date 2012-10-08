#include <Wire.h>
//this library is used to read out the light sensors (tsl 2561) data
//more info:  https://github.com/adafruit/TSL2561-Arduino-Library
#include <TSL2561.h>
#include <Servo.h>
#include <string.h>



void addLight();
void autoExpose( struct FilmType );
void autoExpose( long duration, struct FilmType );
void expose( long exposureTime);


//Hardware
Servo servo;
TSL2561 tsl(TSL2561_ADDR_FLOAT); 
int button = 8;
//LiquidCrystal lcd(7, 8, 9, 10, 11, 12);

//Messung mit Canon, f20, autozeit, iso100... Zeit 5 Sekunden
//Messung gleiche Zeit 5 Sekunden bei Sensor = 175 (accumulatedLight)
//also 35 Lichteinheiten pro Sekunden sind optimal laut dieser Messung (für die Canon)
//müssen dies nun für die Lochkamera berechnen + Film

//die Lochkamera habe eine Lochgröße von 0.25mm, eine Brennweite von 25mm, somit eine Blende von f100
//mit der Formel (gemessene Zeit bei SLR) * (lochkamerablende / SLR-Blende)^2
//ergibt sich eine optimale Belichtungszeit von __125 Sekunden__ in diesem Fall (5sekunden) * (100 / 20)^2

//anschliessend muessen wir noch den Schwrzschildeffekt beachten, der die Belichtungszeit erhöht (eine Blendenstufe == doppelte Belichtungszeit
//bei CHS100
//Gemessene Belichtungszeit (s)	 1/10000 - 1/2	 1	 10	 100
//Belichtungskorrektur (Blenden)	 0	 +1.2	 +1.3	 +1.5

//also bei 125 Sekunden etwa: 125 * (1.5*2) = 375
//Aufgrund von Tests reicht aber dieser Wert noch nicht ganz aus, es empfiehlt sich das Ergebnis noch mit 1,5 bis 2.1 zu multiplizieren
//also 375 * 2 = 750 sekunden (12,5 minuten)
//nun berechnen wir also 750sekunden * 35 Lichteinheiten (die pro sekunde als optimal gemessen wurden)
// == 26250 Lichteinheiten

//wenn wir also 35 Lichteinheiten pro sekunde bekommen, haben wir nach den 12,5 minuten (die wir als optimal ausgerechnet haben, anhand der Daten von Lochkamera
//und mit der DSLR gemessenen Zeit) die 26250 Lichteinheiten gesammelt und müssten ein optimal Belichtetes Negativ haben

//somit Stellen wir fest: unser Maximalwert für einem chs100 bei Blende 100 sind ~27000 (27k)

boolean checkExposure(FilmType);

struct FilmType{
  float  maxLight;
  float reciprocityFailure1s;
  float reciprocityFailure10s;
  float reciprocityFailure100s;
  float reciprocityFailure1000s;
};

int exposureTime=0;
short autoExposure=0;
short i=0;
short isOpen=0;

unsigned long maxLight=0;
float lichtWert;
float accumulatedLight = 0.0;
struct FilmType chs50, chs100;

long time = 0;
int buttonState = 1;

char buffer[128];
char commandB;


/* TODO!

Belichtung für Orthopan anpassen (ca. 1/3 längere Belichtungszeit weil Rot fehlt

*/



void setup() {  

 //setup film types 
 //siehe erklaerung oben
  chs50.reciprocityFailure1s    = 1.2;
  chs50.reciprocityFailure10s   = 1.3; 
  chs50.reciprocityFailure100s  = 1.5;
  chs50.reciprocityFailure1000s = 1.8;
  chs50.maxLight                = 35000; 
  
  chs100.reciprocityFailure1s    = 1.2;
  chs100.reciprocityFailure10s   = 1.3; 
  chs100.reciprocityFailure100s  = 1.5;
  chs100.reciprocityFailure1000s = 1.9;
  chs100.maxLight                = 27000; 
  
  Serial.begin(9600);
    pinMode(button, INPUT); 
    if (tsl.begin()) {
    Serial.println("Found sensor");
    //lcd.print("found sensor!");

  } else {
    Serial.println("No sensor?");
    while (1);
  }
  
  servo.attach(2);
    // You can change the gain on the fly, to adapt to brighter/dimmer light situations
  // tsl.setGain(TSL2561_GAIN_0X);         // set no gain (for bright situtations)
   tsl.setGain(TSL2561_GAIN_0X);      // set 16x gain (for dim situations)
  
  // Changing the integration time gives you a longer time over which to sense light
  // longer timelines are slower, but are good in very low light situtations!
  //tsl.setTiming(TSL2561_INTEGRATIONTIME_13MS);  // shortest integration time (bright light)
  tsl.setTiming(TSL2561_INTEGRATIONTIME_101MS);  // medium integration time (medium light)
  //tsl.setTiming(TSL2561_INTEGRATIONTIME_402MS);  // longest integration time (dim light)
  
  
  
  servo.write(0);
  
  
}


void reset(){
   servo.write(0);
   digitalWrite(4,LOW);
   accumulatedLight = 0.0;
   lichtWert = 0;
   maxLight = 0;
   time = 0;
}

void addLight(){
  accumulatedLight =  (accumulatedLight + (float) (tsl.getLuminosity(TSL2561_VISIBLE) / 4.95) );
  // /4.95 because every 202ms or ~1/5s second checks for light ... 10.1s because tsl needs  101ms to sense light x 2 , so 202ms
  //if newLight is not greater 0, then newlight is atleast 1
  time = time + 202;

}

boolean checkExposure(struct FilmType *filmType){

 float  maxl = filmType->maxLight;
 
  
//returns true if the accumulated Light is still less then our maximum
  if (time >= 1000 && time <= 10000){
    return (accumulatedLight  <= (maxl * filmType->reciprocityFailure1s));
    
  } else if (time >= 10000 && time <= 100000){
    return (accumulatedLight  <= (maxl * filmType->reciprocityFailure10s)) ;
    
  } else if (time >= 100000 && time <= 10000000){
    return (accumulatedLight <= (maxl * filmType->reciprocityFailure100s)) ;
    
    
  } else if (time >= 1000000 && time <= 100000000){
     return (accumulatedLight <= (maxl * filmType->reciprocityFailure1000s)) ;
    
  } else if (time >= 100000000){
    //versuchen schwarzschildeffekt vrherzusagen mit faktor 0.2 zu addieren je 10000sekunden
    return (accumulatedLight <= (maxl * (filmType->reciprocityFailure100s + 0.2 * (time / 10000) ))) ? true : false ; 
  } else if (time <= 1000){
    return (accumulatedLight <= maxl) ? true : false ;
  }
}


//this is only a testing function to check if the light sensor and the motor are working
//it will open/close the aperture depending on light intensitivity
void changeApertureByLight(){
  lichtWert = tsl.getLuminosity(TSL2561_VISIBLE);
  Serial.print(lichtWert);
  Serial.print("\n");
  if(lichtWert > 350){
    digitalWrite(4,HIGH);
    servo.write(0);
  }
  else{
    digitalWrite(4,LOW); 
    servo.write(170);
  } 
}

void expose(long duration){
  reset();
  Serial.print("\ntime exposure\n shooting for milliseconds: ");
    Serial.print(duration);
    Serial.print("\n");
    
   digitalWrite(4,HIGH);
   servo.write(-20);
    
  while(time < duration){
    addLight();
    Serial.print(duration/time*100);
    Serial.print("%\n");
    
  }
  
  Serial.print("\nfinished\naccumulated light in ");
  Serial.print((float) time/1000);
  Serial.print(" seconds = ");
  Serial.print((float) accumulatedLight);
  Serial.print(" \n ");
  reset();
 
}

void autoExpose(struct FilmType *filmType){
   Serial.print("shooting now, auto light, no time limit");
   digitalWrite(4,HIGH);
   Serial.print(filmType->maxLight);
   servo.write(90);
   while(checkExposure(filmType)){
     addLight();
     Serial.print((float) (accumulatedLight/filmType->maxLight) * 100);
     Serial.print("%\n");
   }

   Serial.print("Exposure time ins seconds: ");
   Serial.print(time/1000);
   reset();  
  
  
}

void autoExpose(long duration, struct FilmType *filmType){
   Serial.print("shooting now, auto light, but with time limit");
   digitalWrite(4,HIGH);
   servo.write(90);
   
   while(checkExposure(filmType) && (time <= duration)){
     addLight();
     Serial.print((float) (accumulatedLight/filmType->maxLight) * 100);
     Serial.print("%\n");
   }
   Serial.print("Exposure time ins seconds: ");
   Serial.print(time/1000);
   reset();
 
}

int receiveMessage(){
  memset(buffer,'\0',128);
  Serial.readBytesUntil('!',buffer,120);
  delay(50);

  if(buffer[0] == 'E') {
    sscanf(buffer,"E%d %d %d",&exposureTime,&autoExposure, &maxLight);

    Serial.print(exposureTime);
    return 1;
    //exit(0);
  } else if (buffer[0] == 'A'){
    
    return 2;
  } else {
    return 0;
  }

}


void loop() {
  /*
  
  checking for serial/usb input
  to start exposure via usb send:
  
  E<maxTime> <maxExposure> <maxLight>
  
  or for full automation depending on the film type:
  
  A
  
  */
  
  if(Serial.available()){
    int erg=receiveMessage();
    if (erg == 1 ){
      if (autoExposure && exposureTime > 0){
        Serial.print("autoexpose\n");
       autoExpose(exposureTime,&chs100);
      } else if (autoExposure && exposureTime == 0){
        autoExpose(&chs100);
      }else {
      Serial.print("timeExposure\n");
      expose(exposureTime);
      }
    } else if (erg == 2 ){
      Serial.print("total automatik");
        autoExpose(&chs100);
      
    }
    
  }

}





