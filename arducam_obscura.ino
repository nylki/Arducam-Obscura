  
  
  //this project is licensed under the GPLv3
  //by Tom Brewe
  
  /*
  This program is used to controll an arduino, connected to a servo motor,
  and a TSL2561 light sensor to act as a analog photographic camera
  with automatic expose.
  The servo will act as shutter. The light sensor helps to estimate remaining
  exposure time of the film.
  For more information on this project, please take a look at the article
  on my website: http://tombr.de/arducam-obscura/
  It is on German, but images are included.

  */
  
  //this library is used to read out the light sensors (tsl 2561) data
  //more info:  https://github.com/adafruit/TSL2561-Arduino-Library
  #include <Wire.h>
  #include <Adafruit_Sensor.h>
  #include <Adafruit_TSL2561.h>
  #include <Servo.h>
  #include <string.h>
  #include <stdlib.h>
  #include <EEPROM.h>

  void addLight(int millisecondsPassed);
  void autoExpose( struct FilmType );
  void autoExpose( unsigned long duration, struct FilmType );
  void expose( unsigned long exposureTime);
  
  
  //Hardware
  Servo servo;
  Adafruit_TSL2561 tsl = Adafruit_TSL2561(TSL2561_ADDR_FLOAT, 12345);
  int button = 7;
  //LiquidCrystal lcd(7, 8, 9, 10, 11, 12);
  
  //Messung mit Canon, f20, autozeit, iso100... Zeit 5 Sekunden
  //Messung gleiche Zeit 5 Sekunden bei Sensor = 175 (accumulatedLight)
  //also 35 Lichteinheiten pro Sekunden sind optimal laut dieser Messung (f√ºr die Canon)
  //m√ºssen dies nun f√ºr die Lochkamera berechnen + Film
  
  //die Lochkamera habe eine Lochgr√∂√üe von 0.25mm, eine Brennweite von 25mm, somit eine Blende von f100
  //mit der Formel (gemessene Zeit bei SLR) * (lochkamerablende / SLR-Blende)^2
  //ergibt sich eine optimale Belichtungszeit von __125 Sekunden__ in diesem Fall (5sekunden) * (100 / 20)^2
  
  //anschliessend muessen wir noch den Schwrzschildeffekt beachten, der die Belichtungszeit erh√∂ht (eine Blendenstufe == doppelte Belichtungszeit
  //bei CHS100
  //Gemessene Belichtungszeit (s)	 1/10000 - 1/2	 1	 10	 100
  //Belichtungskorrektur (Blenden)	 0	 +1.2	 +1.3	 +1.5
  
  //also bei 125 Sekunden etwa: 125 * (1.5*2) = 375
  //Aufgrund von Tests reicht aber dieser Wert noch nicht ganz aus, es empfiehlt sich das Ergebnis noch mit 1,5 bis 2.1 zu multiplizieren
  //also 375 * 2 = 750 sekunden (12,5 minuten)
  //nun berechnen wir also 750sekunden * 35 Lichteinheiten (die pro sekunde als optimal gemessen wurden)
  // == 26250 Lichteinheiten
  
  //wenn wir also 35 Lichteinheiten pro sekunde bekommen, haben wir nach den 12,5 minuten (die wir als optimal ausgerechnet haben, anhand der Daten von Lochkamera
  //und mit der DSLR gemessenen Zeit) die 26250 Lichteinheiten gesammelt und m√ºssten ein optimal Belichtetes Negativ haben
  
  //somit Stellen wir fest: unser Maximalwert f√ºr einem chs100 bei Blende 100 sind ~27000 (27k)
  
  
  /*
  TODO:
   - cleanup! -> use main loop for regular exposure, not extra functions, so having better control and readability
   - log exposure time for later reference (eg. testing film exposures), nice graphs, etc
   - only open aperture when a minimum brightness is present. useful when using a colorfilter. problem is, automatic exposure is unreliable,
     as the sensor only detects the filtered light, but actually more light is entering the camera.
   - 
  */
  
  boolean checkExposure(FilmType);
  
  struct FilmType{
    /* TODO: regarding schwartzschildt: interpolate time after passing last reference time (which atm. is reciprocityFailure1000s)  */
    long  maxLight;
    float reciprocityFailure1s;
    float reciprocityFailure10s;
    float reciprocityFailure100s;
    float reciprocityFailure1000s;
  };
  
  unsigned long exposureTime=0;
  short autoExposure=0;
  short i=0;
  boolean isShooting = false;
  
  float maxLight=0;
  float lichtWert;
  float accumulatedLight = 0.0;
  struct FilmType chs50, chs100, iso200, portra800, delta3200, provia400x;
  int millsecondsSensorDelay = 250;
  unsigned long momentButtonPressed = 0;
  unsigned long buttonPressDuration = 0;
  unsigned long time = 0;
  unsigned long startMillis = 0;
  unsigned long momentLastWrite = 0;
  int buttonStatus = LOW;
  
  char buffer[128];
  char commandB;
  
  /* TODO!
  Belichtung fuer Orthopan anpassen (ca. 1/3 längere Belichtungszeit weil Rot fehlt  
  */
  
  
  //This function will write a 4 byte (32bit) long to the eeprom at
//the specified address to adress + 3.
void EEPROMWritelong(int address, long value)
      {
      //Decomposition from a long to 4 bytes by using bitshift.
      //One = Most significant -> Four = Least significant byte
      byte four = (value & 0xFF);
      byte three = ((value >> 8) & 0xFF);
      byte two = ((value >> 16) & 0xFF);
      byte one = ((value >> 24) & 0xFF);

      //Write the 4 bytes into the eeprom memory.
      EEPROM.write(address, four);
      EEPROM.write(address + 1, three);
      EEPROM.write(address + 2, two);
      EEPROM.write(address + 3, one);
 }
 
 
 long EEPROMReadlong(long address)
      {
      //Read the 4 bytes from the eeprom memory.
      long four = EEPROM.read(address);
      long three = EEPROM.read(address + 1);
      long two = EEPROM.read(address + 2);
      long one = EEPROM.read(address + 3);

      //Return the recomposed long by using bitshift.
      return ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
      }
  
  void displaySensorDetails(void){
  sensor_t sensor;
  tsl.getSensor(&sensor);
  Serial.println("------------------------------------");
  Serial.print  ("Sensor:       "); Serial.println(sensor.name);
  Serial.print  ("Driver Ver:   "); Serial.println(sensor.version);
  Serial.print  ("Unique ID:    "); Serial.println(sensor.sensor_id);
  Serial.print  ("Max Value:    "); Serial.print(sensor.max_value); Serial.println(" lux");
  Serial.print  ("Min Value:    "); Serial.print(sensor.min_value); Serial.println(" lux");
  Serial.print  ("Resolution:   "); Serial.print(sensor.resolution); Serial.println(" lux");  
  Serial.println("------------------------------------");
  Serial.println("");
  //delay(500);
}

void configureSensor(void){
  /* You can also manually set the gain or enable auto-gain support */
  // tsl.setGain(TSL2561_GAIN_1X);      /* No gain ... use in bright light to avoid sensor saturation */
  // tsl.setGain(TSL2561_GAIN_16X);     /* 16x gain ... use in low light to boost sensitivity */
  tsl.enableAutoGain(true);          /* Auto-gain ... switches automatically between 1x and 16x */
  
  /* Changing the integration time gives you better sensor resolution (402ms = 16-bit data) */
  // tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_13MS);      /* fast but low resolution */
  tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_101MS);  /* medium resolution and speed   */
  // tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_402MS);  /* 16-bit data but slowest conversions */

  /* Update these values depending on what you've set above! */  
  Serial.println("------------------------------------");
  Serial.print  ("Gain:         "); Serial.println("Auto");
  Serial.print  ("Timing:       "); Serial.println("101 ms");
  Serial.println("------------------------------------");
}

  void setup() {  
   //setup film types 
   //siehe erklaerung oben 
    chs50.reciprocityFailure1s    = 1.2;
    chs50.reciprocityFailure10s   = 1.3; 
    chs50.reciprocityFailure100s  = 1.5;
    chs50.reciprocityFailure1000s = 1.8;
    chs50.maxLight                = 35000.0; 
    
    chs100.reciprocityFailure1s    = 1.2;
    chs100.reciprocityFailure10s   = 1.3; 
    chs100.reciprocityFailure100s  = 1.5;
    chs100.reciprocityFailure1000s = 1.9;
    chs100.maxLight                = 27000.0; 
    
    iso200.reciprocityFailure1s    = 1.2;
    iso200.reciprocityFailure10s   = 1.3; 
    iso200.reciprocityFailure100s  = 1.5;
    iso200.reciprocityFailure1000s = 1.9;
    iso200.maxLight                = 13500.0; 
    
    portra800.reciprocityFailure1s    = 1.4;
    portra800.reciprocityFailure10s   = 2.0; 
    portra800.reciprocityFailure100s  = 3.5;
    portra800.reciprocityFailure1000s = 4.3;
    portra800.maxLight                = 3500.0; 
    
    // http://www.ilfordphoto.com/Webfiles/201071394723115.pdf
    delta3200.reciprocityFailure1s    = 2.0;
    delta3200.reciprocityFailure10s   = 3.0; 
    delta3200.reciprocityFailure100s  = 7.0;
    delta3200.reciprocityFailure1000s = 12.0;
    delta3200.maxLight                = 875.0;
   
   // http://www.fujifilm.com/products/professional_films/pdf/provia_400x_datasheet.pdf
    provia400x.reciprocityFailure1s   = 1.0;
    provia400x.reciprocityFailure10s   = 1.0; 
    provia400x.reciprocityFailure100s  = 1.3;
    provia400x.reciprocityFailure1000s = 2.1;
    provia400x.maxLight                = 6800.0;
    momentLastWrite = millis();
    startMillis = millis();
    
    Serial.begin(9600);
    pinMode(button, INPUT);
  
    if (tsl.begin()) {
      Serial.println("Found sensor"); 
    } else {
      Serial.println("No sensor?");
      while (1);
    } 
    displaySensorDetails();
    configureSensor();
    servo.attach(4);
    servo.write(0); 
  }
  
  void reset(){
     displaySensorDetails();
     configureSensor();
     servo.write(0);
     accumulatedLight = 0.0;
     lichtWert = 0;
     maxLight = 0;
     time = 0;
     momentButtonPressed = 0;
     buttonPressDuration = 0;
     isShooting = false;
     startMillis = millis();
     momentLastWrite = time;
  }
  
  void checkButton(){
    buttonStatus = digitalRead(button);
    if(buttonStatus == HIGH){
      if (momentButtonPressed == 0) momentButtonPressed = time;
      buttonPressDuration = time - momentButtonPressed;
    }
  }
  
  
  void checkWritePeriodicalData(){
    //write light value to eeprom if last write is 5 minutes ago
    if (time - momentLastWrite > 300000){
      EEPROMWritelong(0, accumulatedLight);
      EEPROMWritelong(1, time);
      momentLastWrite = time;
    }  
  }
  
  void addLight(int millisecondsPassed){
      time += millisecondsPassed;
      sensors_event_t event;
      tsl.getEvent(&event);
      if (event.light){
        //millisecondsPassed is usually 250
        
        float newLight = (float) event.light * ((float) millisecondsPassed/1000);
        //Serial.print("add this much light: ");
        //Serial.println(newLight);
        accumulatedLight += newLight;
        //dividing it, because we want use the light per second
        // /4.95 because every 202ms or ~1/5s second checks for light ... 10.1s because tsl needs  101ms to sense light x 2 , so 202ms
        //if newLight is not greater 0, then newlight is atleast 1
        //Serial.print(event.light); Serial.println(" lux.");
        if(time%1000 == 0){
          //Serial.print("Exposure time ins seconds: "); Serial.print( (float) time/1000);
        }
      } else {
        //Serial.println("Sensor overload");
      }
  
  }
  
  boolean checkExposure(struct FilmType *filmType){
    //first check if button might be pressed
   float  maxl = filmType->maxLight;
   
   //Serial.print("\nlicht gesammelt: ");
   //Serial.print(accumulatedLight);
   Serial.print("\nprozent fertig: ");
   Serial.print((float) (accumulatedLight/filmType->maxLight) * 100);
   Serial.println();
   
   
  //returns true if the accumulated Light is still less then our maximum
    if (time >= 1000L && time <= 10000L){
      return (accumulatedLight  <= (maxl * filmType->reciprocityFailure1s)); 
    } else if (time >= 10000L && time <= 100000L){
      return (accumulatedLight  <= (maxl * filmType->reciprocityFailure10s)) ;
    } else if (time >= 100000L && time <= 10000000L){
      return (accumulatedLight <= (maxl * filmType->reciprocityFailure100s)) ;
    } else if (time >= 1000000L && time <= 100000000L){
       return (accumulatedLight <= (maxl * filmType->reciprocityFailure1000s)) ;
      
    } else if (time >= 100000000L){
      //versuchen schwarzschildeffekt vrherzusagen mit faktor 0.2 zu addieren je 10000sekunden
      return (accumulatedLight <= (maxl * (filmType->reciprocityFailure100s + 0.2 * (time / 10000L) ))) ? true : false ; 
    } else if (time <= 1000L){
      return (accumulatedLight <= maxl) ? true : false ;
    }

  }
  
  void expose(unsigned long duration){
    reset();
    Serial.print("\ntime exposure\n shooting for milliseconds: " + duration);
    digitalWrite(4,HIGH);
    servo.write(-20);
    while(time < duration){
      addLight(millsecondsSensorDelay);
      delay(millsecondsSensorDelay);
    }
    reset();
  }
  
  void autoExpose(struct FilmType *filmType){
     //Serial.println("shooting now, auto light, no time limit");
     digitalWrite(4,HIGH);
     //Serial.println(filmType->maxLight);
     servo.write(90);
     while(checkExposure(filmType)){
      checkButton();
      /*
      Serial.print("buttonPressDuration: ");
      Serial.println(buttonPressDuration);
      */
      // if button was pressed and now not anymore
      if (buttonStatus == LOW && momentButtonPressed > 0) {
        if (buttonPressDuration < 10000 && buttonPressDuration > 3000) {
          EEPROMWritelong(0, accumulatedLight);
          //EEPROMWritelong(1, time);
          reset();
          Serial.println("pausing...");
          return;
        } else if (buttonPressDuration >= 10000){
          Serial.println("resetting...");
          EEPROMWritelong(0, 0);
          reset();
          return;
        }
      }
      
      checkWritePeriodicalData();
      addLight(millsecondsSensorDelay);
      delay(millsecondsSensorDelay);
     }
     
     EEPROMWritelong(0, 0);
     reset();
     return;
  }
  
  void autoExpose(unsigned long duration, struct FilmType *filmType){
     Serial.println("shooting now, auto light, but with time limit");
     digitalWrite(4,HIGH);
     servo.write(90);
     while(checkExposure(filmType) && (time <= duration)){
       checkButton();
       addLight(millsecondsSensorDelay);
       delay(millsecondsSensorDelay);
     }
     reset();
  }
  
  int receiveMessage(){
    memset(buffer,'\0',128);
    Serial.readBytesUntil('!',buffer,120);
    delay(50);
    if(buffer[0] == 'E') {
      sscanf(buffer,"E%d %d %d",&exposureTime,&autoExposure, &maxLight);
      //Serial.println(exposureTime);
      return 1;
      //exit(0);
    } else if (buffer[0] == 'A'){
      return 2;
    } else {
      return 0;
    }
  }
  
  void loop() {
    
    
            unsigned long savedLight = EEPROMReadlong(0);
        
        Serial.print("read from eeprom: ");
        Serial.println(savedLight);
    /*
    
    checking for serial/usb input
    to start exposure via usb send:
    
    E<maxTime> <maxExposure> <maxLight>
    
    or for full automation depending on the film type:  A
    */
    
    
    time = millis() - startMillis;
    /*
    if(Serial.available()){
      int erg=receiveMessage();
      if (erg == 1 ){
        if (autoExposure && exposureTime > 0){
          // Serial.println("autoexpose");
         autoExpose(exposureTime,&chs100);
        } else if (autoExposure && exposureTime == 0){
          autoExpose(&chs100);
        }else {
        //Serial.println("timeExposure");
        expose(exposureTime);
        }
      } else if (erg == 2 ){
        //Serial.println("totale automatik");
        autoExpose(&chs100);  
      }
    }
    */
    
    
    checkButton();
    if (momentButtonPressed > 0 && buttonPressDuration > 1000 && isShooting == false) {
        isShooting = true;
        unsigned long savedLight = EEPROMReadlong(0);
        
        Serial.print("read from eeprom: ");
        Serial.println(savedLight);
        
        //float savedTime = EEPROMReadlong(0);
        if(savedLight > 0){
         //restore from data saved on EEPROM 
          accumulatedLight = savedLight; 
        }
        isShooting = true;
        buttonPressDuration = 0;
        momentButtonPressed = 0;
        autoExpose(&iso200);
    }  
}
  
  
  
  


