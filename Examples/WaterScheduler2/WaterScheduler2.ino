#include <Regexp.h>
#include <EEPROMEx.h>
#include <Time.h>
#include <TimeAlarms2.h>

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
 */

// ID of the settings block
//#define CONFIG_VERSION "v10"
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
//Storage storage[5]; 

int  rows  = 0; //storage rows
bool storageChanged = false;
//bool timeBlink = false;

void setup ()
{
  //Serial.begin (19200);
  Serial.begin (9600);

  Serial.println ();
  pinMode( timeStatusPin, OUTPUT);

  //load config
  rows = loadConfig();
  registerAlarms( storage, rows);

  //set time
  //setSyncProvider( requestSync);  //set function to call when sync required
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
  }

  if(timeStatus() ==  timeNotSet){
    //Serial.println("Waiting for time sync ( T minute hour day month year):");
    digitalWrite( timeStatusPin, HIGH);
    Alarm.delay(200);
    digitalWrite( timeStatusPin, LOW);
  }
  digitalClockDisplay();
  Alarm.delay(1000); // wait one second between clock display

}

void registerAlarms( Storage *storagePtr, int count)
{
  timeDayOfWeek_t dow;

  //Storage *storagePtr;
  //rows = loadConfig();
  //storagePtr = storage;
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
  char charArr[60];
  MatchState ms;
  int i = 0;
  // if time sync available from serial port, update time and return true
  while(Serial.available() > 0){  // time message consists of a header and ten ascii digits
    char c = (char)Serial.read() ; 
    inStr += c;
    //charArr[i++] = c;

    if( c == '\n' ) {
      //charArr[i-1] = '\0';      
      //Serial.println(inStr);
      inStr.trim();
      //String.length(): Returns the length of the String, in characters. (Note that this doesn't include a trailing null character.)
      inStr.toCharArray(charArr, inStr.length()+1);
      charArr[inStr.length()] = '\0';

      //Serial.print("charArr: "); 
      //Serial.println(charArr);
      //Serial.print("inStr.length: "); Serial.println(inStr.length());
      //Serial.print("charArr.length: "); Serial.println(strlen(charArr));
      //MatchState ms(charArr);
      //ms.Target(charArr);
      ms.Target(charArr, inStr.length()+1);

      if(inStr.startsWith( TIME_HEADER)) {
        Serial.println("start time pattern...");
        ms.GlobalMatch( timePattern, time_match_callback);
        rows = loadConfig();
        registerAlarms( storage, rows);
      }      
      else if(inStr.startsWith(ALARM_HEADER)){
        //else if( ms.MatchCount(alarmPattern)> 0){
        Serial.println("start alarm pattern...");
        //ms.GlobalMatch( alarmPattern, alarm_match_callback);
        rows = parseAlarmMessage( charArr, rows);
      }
      else if(inStr.startsWith( ALARM_CLEAR_HEADER)){
        Serial.println("Clear alarm repeat...");
        rows = 0;
        EEPROM.write(0, rows);
        //free alarms
        freeAlarms();
      }
      else if(inStr.startsWith( ALARM_LIST_HEADER)){
        printAlarms( rows);
      }
      
      break;
    }   
  } // end of while loop 

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
  Serial.print("List of Alarms: "); Serial.println( count);
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

}

/* called for each match */

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

int parseAlarmMessage(char *pmsg, int count){
  Storage inStorage;
  char* pstr[8];
  int cap[6];
  char * pch;
  int strLen = 0;
  //int count = rows;

  //printf("Splitting string \"%s\" into tokens:\n",str);
  Serial.print("Splitting string into tokens: "); 
  Serial.println(pmsg);

  int  i = 0, j = 0;
  pch = strtok (pmsg,"A");
  while (pch != NULL)
  {
    pstr[i++] = pch;
    //printf ("%s\n",pch);
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
  //EEPROM.setMaxAllowedWrites( sizeof(StoreStruct) * MAX_STORAGE);
  EEPROM.write(0, (uint8_t)count);
  int configAddress  = EEPROM.getAddress(sizeof(StoreStruct)); // Size of config object
  EEPROM.writeBlock(configAddress, storage, count);

  //Serial.print("configAddress: "); 
  //Serial.println(configAddress);

  for(int i = 0; i < count; i++){
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
/*
String getDayOfWeek(byte dow){
 Serial.print("byte dow: "); Serial.println(dow);
 return getDayOfWeek((int)dow);  
 }
 */
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

time_t requestSync()
{
  Serial.write(TIME_REQUEST);  
  return 0; // the time will be sent later in response to serial mesg
}




