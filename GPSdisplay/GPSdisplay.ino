// This code shows how to listen to the GPS module in an interrupt
// which allows the program to have more 'freedom' - just parse
// when a new NMEA sentence is available! Then access data when
// desired.


#include <Adafruit_GPS.h>
#include <SoftwareSerial.h>
#include <LiquidCrystal.h>

//GPS RX,TX pins connected to Arduino pins 3, 2 (respectively)
SoftwareSerial mySerial(3, 2);

Adafruit_GPS GPS(&mySerial);

// initialize the LCD library with the numbers of the interface pins
LiquidCrystal lcd(7, 8, 9, 10, 11, 12);

// Set GPSECHO to 'false' to turn off echoing the GPS data to the Serial console
// Set to 'true' if you want to debug and listen to the raw GPS sentences. 
#define GPSECHO  false
#define KNOTS_TO_MPH 1.15078
#define KNOTS_TO_KPH 1.852
#define KNOTS_TO_MS 0.514444

// this keeps track of whether we're using the interrupt
// off by default!
boolean usingInterrupt = false;
void useInterrupt(boolean); // Func prototype keeps Arduino 0023 happy

void setup()  
{
  lcd.begin(16, 2);
  lcd.print("Booting GPS");
  // connect at 115200 so we can read the GPS fast enough and echo without dropping chars
  // also spit it out
  //Serial.begin(115200);
  //Serial.println("Adafruit GPS library basic test!");

  // 9600 NMEA is the default baud rate for Adafruit MTK GPS's- some use 4800
  GPS.begin(9600);
  
  // uncomment this line to turn on RMC (recommended minimum) and GGA (fix data) including altitude
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  // uncomment this line to turn on only the "minimum recommended" data
  //GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);
  // For parsing data, we don't suggest using anything but either RMC only or RMC+GGA since
  // the parser doesn't care about other sentences at this time
  
  // Set the update rate
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);   // 1 Hz update rate
  // For the parsing code to work nicely and have time to sort thru the data, and
  // print it out we don't suggest using anything higher than 1 Hz

  // Request updates on antenna status, comment out to keep quiet
  GPS.sendCommand(PGCMD_ANTENNA);

  // the nice thing about this code is you can have a timer0 interrupt go off
  // every 1 millisecond, and read data from the GPS for you. that makes the
  // loop code a heck of a lot easier!
  useInterrupt(true);

  for(int i = 0; i < 3; i++) {
    lcd.print(".");
    delay(333);
  }
  // Ask for firmware version
  mySerial.println(PMTK_Q_RELEASE);
  lcd.setCursor(0, 0);
           //----------------
  lcd.print("Starting Logger");
  while(true) {
    if(!GPS.LOCUS_StartLogger()) {
      delay(100);
    } else {
      break;
    }
  }
}


// Interrupt is called once a millisecond, looks for any new GPS data, and stores it
SIGNAL(TIMER0_COMPA_vect) {
  char c = GPS.read();
  // if you want to debug, this is a good time to do it!
#ifdef UDR0
  if (GPSECHO)
    if (c) UDR0 = c;  
    // writing direct to UDR0 is much much faster than Serial.print 
    // but only one character can be written at a time. 
#endif
}

void useInterrupt(boolean v) {
  if (v) {
    // Timer0 is already used for millis() - we'll just interrupt somewhere
    // in the middle and call the "Compare A" function above
    OCR0A = 0xAF;
    TIMSK0 |= _BV(OCIE0A);
    usingInterrupt = true;
  } else {
    // do not call the interrupt function COMPA anymore
    TIMSK0 &= ~_BV(OCIE0A);
    usingInterrupt = false;
  }
}

void printPadded(int number, int width, char pad) {
  if(number > 0) {
    for(int i = 1; i < width - log10(number); i++) {
      lcd.print(pad);
    }
    lcd.print(number, DEC);
  } else {
    for(int i = 1; i < width; i++) {
      lcd.print(pad);
    }
    lcd.print('0');
  }
}

void printPadded(int number, int width) {
  printPadded(number, width, '0');
}

uint32_t timer = millis();
long fixStartTime = -1;
long fixEndTime = -1;
void loop()                     // run over and over again
{
  // if a sentence is received, we can check the checksum, parse it...
  if (GPS.newNMEAreceived()) {
    // a tricky thing here is if we print the NMEA sentence, or data
    // we end up not listening and catching other sentences! 
    // so be very wary if using OUTPUT_ALLDATA and trytng to print out data
    //Serial.println(GPS.lastNMEA());   // this also sets the newNMEAreceived() flag to false
  
    if (!GPS.parse(GPS.lastNMEA()))   // this also sets the newNMEAreceived() flag to false
      return;  // we can fail to parse a sentence in which case we should just wait for another
  }

  // if millis() or timer wraps around, we'll just reset it
  if (timer > millis())  timer = millis();
  
  if (fixStartTime < 0) fixStartTime = millis(); //only the first time

  if(millis() - timer > 1000) {
    timer = millis();
    if(GPS.fix) {
      if(fixEndTime < 0) {
        fixEndTime = millis();
        lcd.clear();
        lcd.print("TTF: ");
        lcd.print((fixEndTime - fixStartTime) / 1000);
        lcd.print("s");
      }
      
      lcd.setCursor(0, 1);
      lcd.print(GPS.speed * KNOTS_TO_MPH, 2);
      lcd.print("mph");
      
      //printPadded(GPS.hour, 2); lcd.print(':');
      //printPadded(GPS.minute, 2); lcd.print(':');
      //printPadded(GPS.seconds, 2);

      //lcd.setCursor(9, 1); //8chars for time + 2 space
      if(GPS.LOCUS_ReadStatus()) {
        int percent = (int)GPS.LOCUS_percent;
        //0123456789ABCDEF
        //TTF: 999s   100%
        lcd.setCursor(12, 0);
        printPadded(percent, 3, ' ');
        lcd.print('%');
      }
    } else {
      //did we have a fix before?
      if(fixEndTime > 0) {
        fixEndTime = -1;
        fixStartTime = millis();
      }
      lcd.clear();
      lcd.print("GPS locating...");
      lcd.setCursor(0, 1);
      lcd.print((millis() - fixStartTime) /1000);
    }
  }

  /*
  // approximately every 2 seconds or so, print out the current stats
  if (millis() - timer > 2000) { 
    timer = millis(); // reset the timer
    
    Serial.print("\nTime: ");
    Serial.print(GPS.hour, DEC); Serial.print(':');
    Serial.print(GPS.minute, DEC); Serial.print(':');
    Serial.print(GPS.seconds, DEC); Serial.print('.');
    Serial.println(GPS.milliseconds);
    Serial.print("Date: ");
    Serial.print(GPS.day, DEC); Serial.print('/');
    Serial.print(GPS.month, DEC); Serial.print("/20");
    Serial.println(GPS.year, DEC);
    Serial.print("Fix: "); Serial.print((int)GPS.fix);
    Serial.print(" quality: "); Serial.println((int)GPS.fixquality); 
    if (GPS.fix) {
      Serial.print("Location: ");
      Serial.print(GPS.latitude, 4); Serial.print(GPS.lat);
      Serial.print(", "); 
      Serial.print(GPS.longitude, 4); Serial.println(GPS.lon);
      Serial.print("Location (in degrees, works with Google Maps): ");
      Serial.print(GPS.latitudeDegrees, 4);
      Serial.print(", "); 
      Serial.println(GPS.longitudeDegrees, 4);
      
      Serial.print("Speed (knots): "); Serial.println(GPS.speed);
      Serial.print("Angle: "); Serial.println(GPS.angle);
      Serial.print("Altitude: "); Serial.println(GPS.altitude);
      Serial.print("Satellites: "); Serial.println((int)GPS.satellites);
    }
  }
  */
}
