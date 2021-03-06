//#include <Regexp.h>
#include <EEPROMEx.h>
#include <Time.h>
#include <TimeAlarms2.h>
#include <Wire.h>  
#include <DS1307RTC.h>  // a basic DS1307 library that returns time as a time_t

//#include <SPI.h>
//#include <WiFi.h>

/*
Patterns
 
 The standard patterns (character classes) you can search for are:
 
 . --- (a dot) represents all characters. 
 %a --- all letters. 
 %c --- all control characters. 
 %d --- all digits. 
 %l --- all lowercase letters. 
 %p --- all punctuation characters. 
 %s --- all space characters. 
 %u --- all uppercase letters. 
 %w --- all alphanumeric characters. 
 %x --- all hexadecimal digits. 
 %z --- the character with hex representation 0x00 (null). 
 %% --- a single '%' character.
 %1 --- captured pattern 1.
 %2 --- captured pattern 2 (and so on).
 %f[s]  transition from not in set 's' to in set 's'.
 %b()   balanced pair ( ... ) 
 
 http://www.gammon.com.au/scripts/doc.php?lua=string.find
 http://arduino.cc/forum/index.php?topic=59917
 
 Circuit:
 * WiFi shield attached
 *  only output digital pin 3, 5, 6, 8, 9  because of WiFi shield pins: 
 *  13:SCK, 12: MISO, 11: MOSI, 10: SS for WiFi, 7: Handshake between shield and Arduino,
 *   4: SS for SD card
 
 Time message: "T minute hour day month year" --> "T 26 16 10 4 13" = 16:26 2013-4-10
 Alarm message: "A id minute hour weekday(sunday:1) duration(minute) pin#" --> "A 1 26 16 4 2 8" = 16:26, Wednesday(4), 2(minute), 8(pin#)"  
 Clear Alarm message: "C *"--> Clear all or "C id" --> Clear Alarm of id  
 List of Alarms message: "L"
 *
 */

// Tell it where to store your config data in EEPROM
#define memoryBase 2

#define TIME_HEADER  "T"   // Header tag for serial time sync message: T minute hour day month
#define TIME_REQUEST  7    // ASCII bell character requests a time sync message 
#define ALARM_HEADER "A"  // Header tag for serial alarm message: A minute hour weekday duration pin#
#define ALARM_CLEAR_HEADER "C"  //Header tag for serial alarm clear
#define ALARM_LIST_HEADER "L"
#define MAX_STORAGE  7  //dtNBR_ALARMS

#define timeStatusPin  13

// Example settings structure
typedef struct StoreStruct {
  //char version[4];   // This is for mere detection if they are your settings
  uint8_t id;
  uint8_t minute, hour, weekday, duration, pin;          // weekday: sunday(1)
} 
Storage;

// serial time text: "T minute hour day month year"
const char  timePattern[] = "T%s*(%d+)%s+(%d+)%s+(%d+)%s+(%d+)%s+(%d+)";
// serial alarm text: A id minute hour dayOfWeek(Sun:1) duration  pin#
const char  alarmPattern[] = "A%s*(%d+)%s+(%d+)%s+(%d+)%s+(%d+)%s+(%d+)%s+(%d+)";
void copyStorage(Storage *target, Storage *source);
void registerAlarms( Storage *storagePtr, int count);

Storage storage[MAX_STORAGE]; 

int  rows  = 0; //storage rows
bool storageChanged = false;

/*
char ssid[] = "baobab";      //  your network SSID (name) 
 char pass[] = "secretPassword";   // your network password
 int keyIndex = 0;                 // your network key Index number (needed only for WEP)
 int status = WL_IDLE_STATUS;
 
 WiFiServer server(80);
 */
void setup ()
{
  //Serial.begin (19200);
  Serial.begin (9600);

  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }
  /* 
   // check for the presence of the shield:
   if (WiFi.status() == WL_NO_SHIELD) {
   Serial.println("WiFi shield not present"); 
   // don't continue:
   while(true);
   } 
   
   // attempt to connect to Wifi network:
   while ( status != WL_CONNECTED) {
   Serial.print("Attempting to connect to SSID: ");
   Serial.println(ssid);
   // Connect to WPA/WPA2 network. Change this line if using open or WEP network:    
   //status = WiFi.begin(ssid, pass);
   status = WiFi.begin(ssid);
   // wait 10 seconds for connection:
   Alarm.delay(2000);
   } 
   
   server.begin();
   // you're connected now, so print out the status:
   printWifiStatus();
   */
  pinMode( timeStatusPin, OUTPUT);

  //load config
  rows = loadConfig();
  registerAlarms( storage, rows);

  //set time
  setSyncProvider( RTC.get);  //set function to call when sync required
  if(timeStatus()!= timeSet) 
    Serial.println("Unable to sync with the RTC");
  else
    Serial.println("RTC has set the system time....");     

  Serial.println("Waiting for time sync( T minute hour day month year):");
}  // end of setup  

void loop() {
  // put your main code here, to run repeatedly: 
  if(Serial.available() > 0){
    processSyncMessage(); 

    if(storageChanged){
      storageChanged = false;
      saveConfig( rows);
      registerAlarms( storage, rows);
    }
  }  //end of serial.available()
  /* 
   // listen for incoming clients
   WiFiClient client = server.available();
   if (client) {
   Serial.println("WiFi Server available...");
   processHttpMessage(client);
   // give the web browser time to receive the data
   Alarm.delay(1);
   // close the connection:
   client.stop();
   Serial.println("client disonnected");
   }
   */
  if(timeStatus() ==  timeNotSet){
    //Serial.println("Waiting for time sync ( T minute hour day month year):");
    digitalWrite( timeStatusPin, HIGH);
    Alarm.delay(200);
    digitalWrite( timeStatusPin, LOW);
  }
  digitalClockDisplay();
  Alarm.delay(1000); // wait one second between clock display

}
/*
void processHttpMessage(WiFiClient client){
 Serial.println("new client");
 // an http request ends with a blank line
 boolean currentLineIsBlank = true;
 while (client.connected()) {
 if (client.available()) {
 char c = client.read();
 Serial.write(c);
 
 // if you've gotten to the end of the line (received a newline
 // character) and the line is blank, the http request has ended,
 // so you can send a reply
 if (c == '\n' && currentLineIsBlank) {
 // send a standard http response header
 Serial.println("send a standard http response header...");
 client.print(F("HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n<html>"));
 
 client.println(F("<head><script type=\"text/javascript\">"));
 client.println(F("function show_alert() {alert(\"This is an alert\");}"));
 client.println(F("</script></head"));
 client.println(F("<body><H1>TEST</H1>"));
 client.println(F("<form method=GET onSubmit=\"show_alert()\">T: <input type=text name=t><br>"));
 client.println(F("R: <input type=text name=r><br><input type=submit></form>"));
 // output the value of each storage info
 client.println(F("<table border=1 style='border:3px;span:5px;'><tr><th>Id</th><th>Time</th><th>Weekday</th><th>Duration</th><th>Pin#</th></tr>"));
 for (int i = 0; i < rows; i++) {
 client.print(F("<tr><td>"));
 client.print(storage[i].id);
 client.print(F("</td><td>"));
 client.print(storage[i].hour);
 client.print(F(":"));
 client.print(storage[i].minute);
 client.print(F("</td><td>"));
 client.print(storage[i].weekday);
 client.print(F("</td><td>"));
 client.print(storage[i].duration);
 client.print(F("</td><td>"));
 client.print(storage[i].pin);
 client.println(F("</td></tr>"));
 }
 client.println(F("</table>"));
 client.println(F("</body></html>"));
 //client.stop();
 
 
 //          client.println("HTTP/1.1 200 OK");
 //          client.println("Content-Type: text/html");
 //          client.println("Connnection: close");
 //          client.println();
 //          client.println("<!DOCTYPE HTML>");
 //          client.println("<html><head>");
 //          // add a meta refresh tag, so the browser pulls again every 5 seconds:
 //          //client.println("<meta http-equiv=\"refresh\" content=\"10\">");
 //          // output the value of each storage info
 //          client.println("</head><body><table border=1 style='border:3px;span:5px;'><tr><th>Id</th><th>Time</th><th>Weekday</th><th>Duration</th><th>Pin#</th></tr>");
 //          for (int i = 0; i < rows; i++) {
 //            client.print("<tr><td>");
 //            client.print(storage[i].id);
 //            client.print("</td><td>");
 //            client.print(storage[i].hour);
 //            client.print(":");
 //            client.print(storage[i].minute);
 //            client.print("</td><td>");
 //            client.print(storage[i].weekday);
 //            client.print("</td><td>");
 //            client.print(storage[i].duration);
 //            client.print("</td><td>");
 //            client.print(storage[i].pin);
 //            client.println("</td></tr></table>");
 //          }
 //          
 //          client.println("</body></html>");
 
 break;
 }
 
 if (c == '\n') {
 // you're starting a new line
 currentLineIsBlank = true;
 } 
 else if (c != '\r') {
 // you've gotten a character on the current line
 currentLineIsBlank = false;
 }
 }
 }  
 }
 */
void registerAlarms( Storage *storagePtr, int count)
{
  timeDayOfWeek_t dow;
  //free alarms
  freeAlarms();

  for(int i = 0; i < count; i++){
    switch(storagePtr->weekday){
    case 1:
      dow = dowSunday;
      break;
    case 2:
      dow = dowMonday;
      break;
    case 3:
      dow = dowTuesday;
      break;
    case 4:
      dow = dowWednesday;
      break;
    case 5:
      dow = dowThursday;
      break;
    case 6:
      dow = dowFriday;
      break;
    case 7:
      dow = dowSaturday;
      break;
    default:
      dow = dowInvalid;
      break;
    }

    Serial.print("set alarmRepeat...id: ");
    Serial.println(storagePtr->id);
    // create the alarms 
    Alarm.alarmRepeat( dow, storagePtr->hour,storagePtr->minute,0,weeklyAlarm2, storagePtr);  // h:m:s every dayOfWeek: Saturday(1) 
    storagePtr++;
  } //end of for loop
}  // end of registerAlarm  

/*
 * alarm pattern: "A1 26 16 4 1 12 A2 28 16 4 1 12 A3 30 16 4 2 12 A4 30 11 5 20 12"
 */

void processSyncMessage() {
  String inStr = "";

  // if time sync available from serial port, update time and return true
  while(Serial.available() > 0){  // time message consists of a header and ten ascii digits
    char c = (char)Serial.read() ; 
    inStr += c;
    if( c == '\n' ) {
      //Serial.println(inStr);
      inStr.trim();
      parseMessage( inStr);

      break;
    }   
  } // end of while loop 

}

/*
  Time message: "T minute hour day month year" --> "T 26 16 10 4 13" = 16:26 2013-4-10
 Alarm message: "A id minute hour weekday(sunday:1) duration(minute) pin#" --> "A 1 26 16 4 2 8" = 16:26, Wednesday(4), 2(minute), 8(pin#)"  
 Clear Alarm message: "C *"--> Clear all or "C id" --> Clear Alarm of id  
 List of Alarms message: "L"
 */
void parseMessage(String msg){
  char charArr[msg.length()+1];

  //String.length(): Returns the length of the String, in characters. (Note that this doesn't include a trailing null character.)
  msg.toCharArray(charArr, msg.length()+1);
  charArr[msg.length()] = '\0';

  //Time message: "T minute hour day month year" --> "T 26 16 10 4 13" = 16:26 2013-4-10
  if(msg.startsWith( TIME_HEADER)) {
    Serial.println("start time pattern...");
    //MatchState ms;
    //ms.Target(charArr, msg.length()+1);
    //ms.GlobalMatch( timePattern, time_match_callback);
    parseTimeMessage(msg);
    rows = loadConfig();
    registerAlarms( storage, rows);
  } //Alarm message: "A id minute hour weekday(sunday:1) duration(minute) pin#" --> "A 1 26 16 4 2 8" = 16:26, Wednesday(4), 2(minute), 8(pin#)"       
  else if(msg.startsWith(ALARM_HEADER)){
    //else if( ms.MatchCount(alarmPattern)> 0){
    Serial.println("start alarm pattern...");
    //ms.GlobalMatch( alarmPattern, alarm_match_callback);
    rows = parseAlarmMessage( charArr, rows);
  } //Clear Alarm message: "C *"--> Clear all or "C id" --> Clear Alarm of id  
  else if(msg.startsWith( ALARM_CLEAR_HEADER)){
    if(msg.endsWith("*")){ // Clear All Alarms
      rows = 0;
      EEPROM.write(0, rows);
      //free alarms
      freeAlarms();
      Serial.println("Clear Alarms...");
    } 
    else { // Clear Alarm of id
      //the string doesn't start with a integral number, a zero is returned.
      long id = msg.substring(1).toInt();
      if(id > 0){
        for(int i =0; i < rows; i++){
          if( id == storage[i].id){
            storage[i].id = 0;
            Alarm.free(i);
          }
        }
        saveConfig( rows);
      }
    }
  } // List of Alarms message: "L"
  else if(msg.startsWith( ALARM_LIST_HEADER)){
    printAlarms( rows);
  }  
}

void freeAlarms(){
  //free alarms
  int count = dtNBR_ALARMS; //Alarm.count();
  Serial.print("Free Alarm Count: "); 
  Serial.println(count);

  for(int id =0;  id < count; id++){
    Alarm.free(id);
  }
}

void printAlarms(int count){
  Serial.print("List of Alarms: "); 
  Serial.println( count);
  for(int i = 0; i < count; i++){
    Serial.print( "id: "); 
    Serial.println(storage[i].id);
    Serial.print("minute: "); 
    Serial.println(storage[i].minute);
    Serial.print("hour: "); 
    Serial.println(storage[i].hour);
    Serial.print("weekday: "); 
    Serial.println(storage[i].weekday);
    Serial.print("duration: "); 
    Serial.println(storage[i].duration);
    Serial.print("pin#: "); 
    Serial.println(storage[i].pin);
    Serial.println();
  }
}

/* called for each time match, call setTime() */
/*
void time_match_callback(const char * match,          // matching string (not null-terminated)
 const unsigned int length,   // length of matching string
 const MatchState & ms)      // MatchState in use (to get captures)
 {
 int minute, hour, day, month, year;
 char cap[5]; //must be large enough to hold capture
 
 Serial.print ("Matched: ");
 Serial.write ((byte *) match, length);
 Serial.println ();
 
 for (byte i = 0; i < ms.level; i++){
 Serial.print ("Capture "); 
 Serial.print (i, DEC);
 Serial.print (":");
 Serial.println (ms.GetCapture (cap, i));
 
 switch(i){
 case 0: 
 minute = atoi(cap);
 break;
 case 1: 
 hour =  atoi(cap);
 break;
 case 2: 
 day =  atoi(cap);
 break;
 case 3: 
 month = atoi(cap);
 break;
 case 4: 
 year =  atoi(cap);
 break;
 } 
 
 }  // end of for each capture
 
 //setTime(int hr,int min,int sec,int day, int month, int yr);
 setTime(hour,minute,0, day,month,year); // set time to Saturday 16:25:50 10 sep 2013
 RTC.set( now());   // set the RTC and the system time to the received value
 }
 */
/* called for each match */
/*
void alarm_match_callback(const char * match,          // matching string (not null-terminated)
 const unsigned int length,   // length of matching string
 const MatchState & ms)      // MatchState in use (to get captures)
 {
 Storage inStorage;
 char cap [6];   // must be large enough to hold captures
 int count = rows;
 
 Serial.print ("Matched: ");
 Serial.write ((byte *) match, length);
 Serial.println ();
 //String(100+rows, DEC).toCharArray(storage[rows].id, 4);
 //Serial.print("id: "); Serial.println(storage[rows].id);
 
 for (byte i = 0; i < ms.level; i++)
 {
 Serial.print ("Capture "); 
 Serial.print (i, DEC);
 Serial.print (":");
 Serial.println( ms.GetCapture (cap, i));
 
 switch(i){
 case 0: 
 //Serial.print("cap0: "); Serial.println(cap);
 //strncpy(storage[rows].id, cap, 3);
 //storage[rows].id[3] = '\0';
 //String(cap).toCharArray(storage[rows].id, 4);
 inStorage.id = atoi(cap);
 break;
 case 1: 
 inStorage.minute =  atoi(cap);
 break;
 case 2: 
 inStorage.hour =  atoi(cap);
 break;
 case 3: 
 inStorage.weekday = atoi(cap);
 break;
 case 4: 
 inStorage.duration =  atoi(cap);
 break;
 case 5: 
 inStorage.pin =  atoi(cap);
 break;
 } 
 
 }  // end of for each capture
 
 //update or add alarm storage
 byte j = 0;
 for(j =0; j < count; j++){
 if( inStorage.id == storage[j].id){
 Serial.print(" update alarm repeat...id:"); 
 Serial.println( inStorage.id);
 copyStorage( &storage[j], &inStorage);
 storageChanged = true;
 break; 
 }
 } // end of for loop
 Serial.print("callback count: "); 
 Serial.println(count);
 
 if( j >= count && j < MAX_STORAGE){
 Serial.println(" add new alarm repeat...");
 copyStorage( &storage[j], &inStorage); 
 //Serial.print("storage[j].id:"); Serial.println(storage[j].id);
 rows++;
 Serial.print("callback rows: "); 
 Serial.println(rows);
 storageChanged = true;
 }
 else if(j >= MAX_STORAGE){
 Serial.println("==Overflow dtNBR_ALARMS==");
 storageChanged = false;
 }
 
 }  // end of match_callback 
 */

void parseTimeMessage(String msg){
  char* pstr[8];
  int cap[6];
  char * pch;
  char charArr[30];

  msg = msg.substring(1); // T 26 8 28 4 2013 = T minute hour day month year
  msg.toCharArray(charArr, msg.length()+1);
  charArr[msg.length()] = '\0';

  Serial.print("Splitting string into tokens: "); 
  Serial.println(charArr);
  int j =0;
  pch = strtok(charArr, " ");
  while(pch != NULL)
  {
    cap[j++] = atoi(pch);
    Serial.println( pch);
    pch = strtok(NULL, " ");
  }
  if(j != 5) {
    Serial.print("Not correct message: "); 
    Serial.println( charArr);
  }

  //setTime(int hr,int min,int sec,int day, int month, int yr);
  setTime(cap[1],cap[0],0, cap[2],cap[3],cap[4]); // set time to Saturday 16:25:50 10 sep 2013
  RTC.set( now());   // set the RTC and the system time to the received value

}

int parseAlarmMessage(String msg, int count){
  Storage inStorage;
  char* pstr[8];
  int cap[6];
  char * pch;
  int strLen = 0;
  char charArr[60];

  msg.toCharArray(charArr, msg.length()+1);
  charArr[msg.length()] = '\0';

  Serial.print("Splitting string into tokens: "); 
  Serial.println(charArr);

  int  i = 0, j = 0;
  pch = strtok (charArr,"A");
  while (pch != NULL)
  {
    pstr[i++] = pch;
    Serial.println( pstr[i-1]);
    pch = strtok (NULL, "A");
  }

  Serial.println("==========");
  strLen = i;
  for(i =0; i < strLen; i++)
  {
    j =0;
    pch = strtok(pstr[i], " ");
    while(pch != NULL)
    {
      cap[j++] = atoi(pch);
      Serial.println( pch);
      pch = strtok(NULL, " ");
    }
    if(j != 6) {
      Serial.print("Not correct message: "); 
      Serial.println( pstr[i]);
    }

    inStorage.id = (cap[0]);
    inStorage.minute =  (cap[1]);
    inStorage.hour =  (cap[2]);
    inStorage.weekday = (cap[3]);
    inStorage.duration =  (cap[4]);
    inStorage.pin =  (cap[5]);
    Serial.print("---id:"); 
    Serial.println(inStorage.id);

    //update or add alarm storage
    int k = 0;
    for(k =0; k < count; k++){
      if( inStorage.id == storage[k].id){
        Serial.print(" update alarm repeat...id:"); 
        Serial.println( inStorage.id);
        copyStorage( &storage[k], &inStorage);
        storageChanged = true;
        break; 
      }
    } // end of for loop
    Serial.print("callback count: "); 
    Serial.println(count);

    if( k >= count && k < MAX_STORAGE){
      Serial.println(" add new alarm repeat...");
      copyStorage( &storage[k], &inStorage); 
      //Serial.print("storage[j].id:"); Serial.println(storage[j].id);
      count++;
      Serial.print("callback rows: "); 
      Serial.println(rows);
      storageChanged = true;
    }
    else if(k >= MAX_STORAGE){
      Serial.println("==Overflow dtNBR_ALARMS==");
      storageChanged = false;
    }
  }
  return count;
}

void copyStorage(Storage *target, Storage *source){
  target->id = source->id;
  target->minute = source->minute;
  target->hour = source->hour;
  target->weekday = source->weekday;
  target->duration = source->duration;
  target->pin = source->pin;
}
/* callback function */
void weeklyAlarm2(void *pUserData){
  Storage *pStorage = (Storage *)pUserData;
  setHigh(pStorage->pin);
  Alarm.timerOnce((pStorage->duration * 60), setLow, pUserData); //second

  Serial.print("weeklyAlarm calling...alarmId: ");
  Serial.println(Alarm.getTriggeredAlarmId());
}

/* callback function */
void setLow(void * pUserData){
  Storage * pStorage = (Storage *)pUserData;
  pinMode(pStorage->pin, OUTPUT);
  digitalWrite(pStorage->pin, LOW);
  Serial.print("setLow calling...alarmId: "); 
  Serial.println(Alarm.getTriggeredAlarmId());
}

void setHigh(int pin){
  pinMode(pin, OUTPUT);
  digitalWrite(pin, HIGH);
  Serial.println("setHigh calling...");
}

void setLow(int pin){
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  Serial.println("setLow calling...");
}

uint8_t loadConfig() {
  EEPROM.setMemPool(memoryBase, EEPROMSizeUno); //Set memorypool base to 2, assume Arduino Uno board
  uint8_t rows = EEPROM.read(0);
  int  configAddress  = EEPROM.getAddress(sizeof(StoreStruct)); // Size of config object
  EEPROM.readBlock(configAddress, storage, rows);

  Serial.print("load storage data... rows: "); 
  Serial.println( rows);
  //Serial.print("configAddress: "); 
  //Serial.println(configAddress);

  printAlarms(rows);
  //return ( !strcmp(storage.id, CONFIG_VERSION));
  return rows;
}

void saveConfig(int count) {
  Serial.println("save storage data..."); 
  Serial.print("EEPROMSizeUno: "); 
  Serial.println(EEPROMSizeUno);
  EEPROM.setMemPool(memoryBase, EEPROMSizeUno); //Set memorypool base to 2, assume Arduino Uno board
  ////EEPROM.setMaxAllowedWrites( sizeof(StoreStruct) * MAX_STORAGE);
  //EEPROM.write(0, (uint8_t)count);
  //int configAddress  = EEPROM.getAddress(sizeof(StoreStruct)); // Size of config object
  //EEPROM.writeBlock(configAddress, storage, count);
  int clearCount = 0;
  for(int i =0; i < count; i++){
    if(storage[i].id == 0){
      clearCount++;
      continue;
    }
    int configAddress  = EEPROM.getAddress(sizeof(StoreStruct)); // Size of config object
    EEPROM.writeBlock(configAddress, storage+i, 1);
  }
  EEPROM.write(0, (uint8_t)(count - clearCount));

  //Serial.print("configAddress: "); 
  //Serial.println(configAddress);
  for(int i = 0; i < count; i++){
    if(storage[i].id == 0) continue;
    Serial.print("id: "); 
    Serial.println(storage[i].id);
    Serial.print("minute: "); 
    Serial.println(storage[i].minute);
    Serial.print("hour: "); 
    Serial.println(storage[i].hour);
    Serial.print("weekday: "); 
    Serial.println(storage[i].weekday);
    Serial.print("duration: "); 
    Serial.println(storage[i].duration);
    Serial.print("pin#: "); 
    Serial.println(storage[i].pin);
  }
}

void digitalClockDisplay()
{
  // digital clock display of the time
  Serial.print("\n");
  Serial.print(hour(), DEC);
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print( getDayOfWeek(weekday()));
  Serial.print("\n"); 

  //digitalWrite( timeStatusPin, timeBlink);
  //timeBlink = !timeBlink;
}

void printDigits(int digits)
{
  Serial.print(":");
  if(digits < 10)
    Serial.print(0, DEC);

  Serial.print(digits, DEC);
}

char* getDayOfWeek(byte dow){
  switch(dow){
  case 1:
    return "Sun";
  case 2:
    return "Mon";
  case 3:
    return "Tue";
  case 4:
    return "Wed";
  case 5:
    return "Thu";
  case 6:
    return "Fri";
  case 7:
    return "Sat";
  default:
    return " * ";
  }
}

/*
void printWifiStatus() {
 // print the SSID of the network you're attached to:
 Serial.print("SSID: ");
 Serial.println(WiFi.SSID());
 
 // print your WiFi shield's IP address:
 IPAddress ip = WiFi.localIP();
 Serial.print("IP Address: ");
 Serial.println(ip);
 
 //print your WiFi shield's Mac address
 printMacAddress();
 
 IPAddress subnet = WiFi.subnetMask();
 Serial.print("Subnet Mask: ");
 Serial.println(subnet);
 
 IPAddress gateway = WiFi.gatewayIP();
 Serial.print("Gateway: ");
 Serial.println(gateway);
 
 
 // print the received signal strength:
 long rssi = WiFi.RSSI();
 Serial.print("signal strength (RSSI):");
 Serial.print(rssi);
 Serial.println(" dBm");
 }
 
 void printMacAddress(){
 	byte mac[6];
 	WiFi.macAddress(mac);
 	Serial.print("MAC: ");
 	for(int i = 5; i >= 0; i--){
 		if(mac[i] < 0x10)
 			Serial.print("0");
 		Serial.print(mac[i], HEX);
 		if( i != 0) 
 			Serial.print(":");
 	}
 Serial.println();
 }
 */

