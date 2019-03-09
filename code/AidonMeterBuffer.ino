/*
  Norwegian AMS power meter sensor/adapter for MySensors and controllers like Domoticz
  
  This is a sensornode for Mysensor, it has a parser that read OBIS codes and meter data from the HAN port of a Norwegian Aidon power meter.
  The parser read the Norwegian HAM code specification - OBIS codes.
  This adapter parses the format according to the format specification and mostly independent of order and content of each message and record.
  Ref https://www.nek.no/wp-content/uploads/2018/11/Aidon-HAN-Interface-Description-v10A-ID-34331.pdf
  and EXCERPT DLMS UA Blue Book: COSEM interface classes and OBIS identification system, EXCERPT DLMS UA 1000-1 Ed. 12.0
  
  Hardware is an arduino that reads a serial data stream form a Mbus to ttl adapter.  
  The MBus converter is connected to the HAN port of the power meter.
  The Adapter is tested with Hafslund Aidon meter, a arduino my sensor node and a Mbus adapter:
  https://www.ebay.com/itm/TSS721A-Breakout-Module-with-Isolation/113359924361?hash=item1a64c72c89:g:gS0AAOSwF31b5paL:rk:4:pf:0

  The arduino MySensor node is connected to a rasperry Domoticz controller via a 2.4 Mhz mesh network. Ref mySensor.org
  
  Keywords: AMS powermeter, HAN port, OBIS, Mbus, COSEM, Domotizc, mysensors
  
  Jon Ola Hove

  Notes:
  Software serial can be used to test the parser,
  but on a 3.3V on 8Mhz this might be to slow when parsing the one hour message, use rx tx. 
  
  The 8mhz arduino is of this type:
  https://forum.mysensors.org/topic/2067/my-slim-2aa-battery-node
  
  This Arduno can probably be powered from the Mbus interface. This is not tested.
  Currently the card is powered with 3.3V via a step-down regulator from 5 v, it consumed 35ma, including loss in the regulator.

  The mysensors data is sent in senddata().

  This parser is a top down recursive parser driven by the grammar of the message,
  specified in the reference above.  The basic grammar is:


  Hdlspackage = startmark, packagelen, frameheader, payload, FCC, endmark
  startmark = endmark = “7E”
  packagelen = 4 byte ; “A”, 12 bit integer
  frameheader = 9 byte ; Not checked, ends with a crc
  payload = dataheader, array, register*
  dataheader = “0f 40 00 00 00 00”  ; might not be fixed
  array = «01», dataLen
  dataLen = 1 byte; Int
  register = structure, structureContent;
  structureContent = octetString, OBIScodeandContent  ; se code for details

  Example:

  Header   a0 2a 41 08 83 13 04 13 e6 e7 00 : FrameType: 10 FrameLength: 42
  DataHeader 0f 40 00 00 00 00
  Type & Len 01 01

  02 03  : Structur – 3
  09 06  : ocet-string len 6
  01 00 01 07 00 ff : OBIS kode  '1.0.1.7.0.255
  06  : double-long-unsigned   (32bit)
  00 00 06 44  : dec 1604
  02 02 : Structur – 2
  0f  Int8
  00 : 0
  16  : enum
  1b : Watt active power
  6f 92  CRC

  ----
  TODO: CRC is not checked,
  test power form the Mbus
  restructure the parser to a c++ class
  There is some isue witk tha last parts of the one hour massage, but it does not effect the data.
*/

#define startMark 0x7E
#define arrayMark  0x01
#define structMark 0x02
#define octetMark  0x09
#define doubelLongUnSignedMark  0x06
#define longSignedMark  0x12
#define longUnSignedMark  0x10
#define scalarMark  0x0F
#define stringMark  0x0a
#define enumMark  0x16


// My sensors
#define MY_RADIO_NRF24

//#define MY_RF24_CHANNEL  84
//###########################
//#define MY_DEBUG
//###########################
// Set LOW transmit power level as default, if you have an amplified NRF-module and
// power your radio separately with a good regulator you can turn up PA level.
#define MY_RF24_PA_LEVEL RF24_PA_HIGH
#define PoWCHILD_ID 1              // Id of the sensor child

#define useMySensors      // If on compile mysensor 

#ifdef useMySensors
#include <MySensors.h>
#endif

//#define useSoftSerial  // to debug the code
//-------------------
// Note: Can not use all debug options simultanious due to memory limitations in arduino
//#define MY_DEBUG2
//#define MY_DEBUG1     // list recieved data form the HAN port on Serial
//#define MY_DEBUG3  
//#define MY_ERROR

#ifdef MY_DEBUG1
#define DEBUG_PRINT(x)     Serial.print (x)
#define DEBUG_CODE(x)     x
#define DEBUG_PRINTDEC(x)     Serial.print (x, DEC)
#define DEBUG_PRINTHEX(x)     Serial.print (x, HEX)
#define DEBUG_PRINTLN(x)  Serial.println (x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_CODE(x)
#define DEBUG_PRINTDEC(x)
#define DEBUG_PRINTHEX(x)
#define DEBUG_PRINTLN(x)
#endif

#ifdef MY_DEBUG2
#define DEBUG2_PRINT(x)     Serial.print (x)
#define DEBUG2_CODE(x)     x
#define DEBUG2_PRINTDEC(x)     Serial.print (x, DEC)
#define DEBUG2_PRINTHEX(x)     Serial.print (x, HEX)
#define DEBUG2_PRINTLN(x)  Serial.println (x)
#define buffMaxLen 340
#else
#define DEBUG2_PRINT(x)
#define DEBUG2_CODE(x)
#define DEBUG2_PRINTDEC(x)
#define DEBUG2_PRINTHEX(x)
#define DEBUG2_PRINTLN(x)
#define buffMaxLen 400
#endif

#ifdef MY_DEBUG3
#define DEBUG3_PRINT(x)     Serial.print (x)
#define DEBUG3_CODE(x)     x
#define DEBUG3_PRINTDEC(x)     Serial.print (x, DEC)
#define DEBUG3_PRINTHEX(x)     Serial.print (x, HEX)
#define DEBUG3_PRINTLN(x)  Serial.println (x)
#else
#define DEBUG3_PRINT(x)
#define DEBUG3_CODE(x)
#define DEBUG3_PRINTDEC(x)
#define DEBUG3_PRINTHEX(x)
#define DEBUG3_PRINTLN(x)

#endif

#ifdef MY_ERROR
#define ERROR_PRINT(x)     Serial.print (x)
#define ERROR_PRINTHEX(x)     Serial.print (x, HEX)
#define ERROR_PRINTLN(x)  Serial.println (x)
#else
#define ERROR_PRINT(x)
#define ERROR_PRINTHEX(x)
#define ERROR_PRINTLN(x)
#endif
//---------------------------------------
#ifdef useSoftSerial  
#include <SoftwareSerial.h>
#endif

//SoftwareSerial mySerial(10, 11); // RX, TX
#ifdef useSoftSerial
SoftwareSerial mySerial(8, 7); // RX, TX
#endif

// temorary values while parsing
int bufferlen; // Length og data package
int bufferPos = 0; // pos while parcing
byte octet[6];
byte datatime[12];
char tempString[20];
int stringLen = 0;
int OBIScode = 0;
unsigned long longValue = 0;
unsigned int unitValue = 0;
int intValue = 0;
int8_t scaleValue = 0;
// Data collected
unsigned long activePowerQ1Q4 = 0;
unsigned long activePowerQ2Q3 = 0;
unsigned long reactivePowerQ1Q2 = 0;
unsigned long reactivePowerQ3Q4 = 0;
float currentL1 = 0;
float currentL2 = 0;
float currentL3 = 0;

byte clockAndTime[12];  // 
float phaseVL1 = 0;
float phaseVL2 = 0;
float phaseVL3 = 0;

unsigned long cumulativActiveIm = 0;
unsigned long cumulativActiveEx = 0;
unsigned long cumulativReactiveIm = 0;
unsigned long cumulativReactiveEx = 0;

char  meterid[20];
char  meterType[20];
char  OBISVersion[20];
byte buffer[buffMaxLen];
;

// My Sensors

#ifdef useMySensors
MyMessage wattMsg(PoWCHILD_ID, V_WATT);
MyMessage kwhMsg(PoWCHILD_ID, V_KWH);
MyMessage reavtMsg(PoWCHILD_ID, V_VAR);  // reactive , finner ikke denne i Domoticz?
MyMessage ampMsg(PoWCHILD_ID, V_VA);  //
MyMessage factorMsg(PoWCHILD_ID, V_POWER_FACTOR);
MyMessage voltMsg(PoWCHILD_ID, V_VOLTAGE);
#endif

// My sensor

#ifdef useMySensors
void presentation()
{
  // Send the sketch version information to the gateway and Controller
  sendSketchInfo("HAN Aidon Energy Meter", "1.0");

  // Register this device as power sensor
  present(PoWCHILD_ID, S_POWER);
}
#endif

void setup() {

  // Open serial communications and wait for port to open:

#ifdef useSoftSerial
  Serial.begin(115200);
#else
  Serial.begin(2400);
#endif
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  DEBUG_PRINT("start:");
  // set the data rate for the SoftwareSerial port
#ifdef useSoftSerial
  mySerial.begin(2400);
#endif

}

void loop() { // run over and over
  parceHDLCPackage();
}

void parceHDLCPackage() {
  while (!testStartMark()) {
  }
  if (!readPackegeLen()) {
    return;
  }
  DEBUG_PRINTLN("start rec: ---");
  readFrame();  // read raw data
  DEBUG_PRINTLN("buffer;");
  DEBUG_CODE(for (int i = 0; i < bufferlen; i++) {if (buffer [i] < 16) Serial.print('0'); DEBUG_PRINTHEX(buffer [i]); });
  DEBUG_PRINTLN();
  if (!testEndMark()) ERROR_PRINT("NoEnd") ;//same mark at end og package

  bufferPos = 2;  // // 2 bytes buffer len
  parseframeHeader();
  parsepayload();
  parseFCC();

  sendData();    // process data */
  DEBUG_CODE(printMsgContent());
  nullValues();
  DEBUG2_PRINTLN();
  DEBUG2_PRINTLN("Next r: ----");

}

void readFrame() {
  for (int i = 2; i < bufferlen; i++) { // 2 bytes buffer len
    buffer [i] = readByte();
  }
}

/* read buffer to slow to pare stream directly, lose bytes */
void parseframeHeader() { //frameheader = 9 byte ;  Fixed for now, last byte is crc
  for (int i = 1; i <= 9; i++) {
    nextBuffByte();
  }
  DEBUG3_PRINT(" End FrH ");
}

boolean parsepayload() { //payload = dataheader, array , register*
  readDataHeader();
  int cardi = readArray();
  for (int i = 1; i <= cardi; i++) {
    int c = nextBuffByte();
    if (c == structMark) {
      DEBUG2_PRINTLN(); DEBUG2_PRINTLN("Reg#:"); DEBUG2_PRINTDEC(i); DEBUG2_PRINTLN();
      readRegister();
      moveData();
      DEBUG_CODE(myprintLastRecord());
      nullTmpValues();
    }
    else {
      ERROR_PRINT("PaylNf"); ERROR_PRINTHEX(c); ERROR_PRINTLN();
    }
  }
  DEBUG2_PRINT("PaylNf"); DEBUG2_PRINTLN();

}

boolean  parseFCC() {   // Todo: calculate chechsum while reading
  DEBUG2_PRINT("FCC"); DEBUG2_PRINTLN();
  nextBuffByte();
  nextBuffByte();
}

void readDataHeader() {  //dataheader = “0f 40 00 00 00 00”  ; might not be fixed
  for (int i = 1; i <= 6; i++) {  // assumed fixed
    nextBuffByte();
  }
  DEBUG3_PRINT(" End DH ");
}

boolean readRegister() { //register = structure, structureContent*
  int cardi = readStructureCrd();
  DEBUG2_PRINT("StrCr:"); DEBUG2_PRINTDEC(cardi); DEBUG2_PRINTLN();

  for (int i = 1; i <= cardi; i++) {
    DEBUG2_PRINT("Str#:"); DEBUG2_PRINTDEC(i); DEBUG2_PRINTLN();
    DEBUG2_PRINTLN();
    registerContent();
  }
}

boolean registerContent() { //structureContent = octetString, OBIScodeandContent or
  // doubelLongUnSignedMark or longUnSignedMark or String or array
  int c = nextBuffByte();  // octet or
  DEBUG2_PRINT("regM:"); DEBUG2_PRINTHEX(c); DEBUG2_PRINTLN();

  switch ( c ) {
    case octetMark  : readOktet(); break;
    case doubelLongUnSignedMark : readDouble(); break;
    case longSignedMark : readLong(); break;
    case longUnSignedMark : readLong(); break;
    case stringMark : myReadString(); break;
    case structMark : readInnerStructure() ; break;  // scale and unit structure
    default:
      ERROR_PRINT(" reg notf"); break;
  }
}

boolean readInnerStructure() { // scaalar , ISO Unit (Enum))
  int cardi = readStructureCrd();
  DEBUG2_PRINT("InnerSt crd:"); DEBUG2_PRINTDEC(cardi); DEBUG2_PRINTLN();

  for (int i = 1; i <= cardi; i++) {
    DEBUG2_PRINT(" ");
    DEBUG2_PRINT("Inner#"); DEBUG2_PRINTDEC(i); DEBUG2_PRINTLN();
    readStructElements();
  }
}

boolean readStructElements() {
  int c = nextBuffByte();  //
  DEBUG2_PRINT("readStructElementsMark: "); DEBUG2_PRINTHEX(c); DEBUG2_PRINTLN();

  switch ( c ) {
    case octetMark  : readDateTime(); break;   // Date and time
    case doubelLongUnSignedMark : readDouble(); break;
    case longSignedMark : readLong(); break;
    case longUnSignedMark : readLong(); break;
    case scalarMark : readScaler(); break;
    case enumMark : readUnit(); break;  // ignore for now, this is ISO unit
    default:
      ERROR_PRINT(" readStrE notF"); ERROR_PRINTHEX(c); break;

  }
}

void myprintLastRecord() {

  DEBUG3_PRINTLN();
  DEBUG3_PRINT("date: ");
  if (datatime[0] != 0) {
    for (int i = 0; i < 12; i++) {
      DEBUG3_PRINTHEX(datatime[i]); DEBUG3_PRINT("-");
    }
  }
  DEBUG3_PRINTLN();
  if (tempString[1] != 0) {
    DEBUG3_PRINT("Str: "); DEBUG3_PRINT(tempString); DEBUG3_PRINTLN();
  }
  if (longValue != 0) {
    DEBUG3_PRINT("LongD32: "); DEBUG3_PRINTDEC(longValue); DEBUG3_PRINTLN();
  }
  if (intValue != 0) {
    DEBUG3_PRINT("Long16: "); DEBUG3_PRINTDEC(intValue); DEBUG3_PRINTLN();
  }
  if (scaleValue != 0) {
    DEBUG3_PRINT("Scale: "); DEBUG3_PRINTDEC(scaleValue); DEBUG3_PRINTLN();
  }
  if (unitValue != 0) {
    DEBUG3_PRINT("Unit: "); DEBUG3_PRINTDEC(unitValue); DEBUG2_PRINTLN();
  }
}

// * decode OBIS and set values */
void moveData() {
  DEBUG3_PRINTLN("Oct: ");
  for (int i = 0; i < 6; i++) {
    DEBUG3_PRINTDEC(octet[i]); DEBUG3_PRINT(".");
  }

  DEBUG3_PRINTLN();
  switch ( octet[0] ) {
    case 0  :
      switch ( octet[2] ) {
        case 96 :
          switch ( octet[4]) {
            case 0 : for (int i = 0; i < stringLen; i++)  meterid[i] = tempString[i];
              DEBUG3_PRINT("meterid: "); DEBUG3_PRINT(tempString);  DEBUG3_PRINTLN(); DEBUG3_PRINT(meterid); break;
            case 7 : for (int i = 0; i < stringLen; i++)  meterType[i] = tempString[i];
              DEBUG3_PRINT("meterT: "); DEBUG3_PRINT(tempString);  DEBUG3_PRINTLN(); DEBUG3_PRINT(meterid); break;
          }; break;
        case 1 :  for (int i = 0; i < 8; i++)  clockAndTime[i] = datatime[i];
          DEBUG3_PRINT("clockTime: ");  DEBUG3_PRINTLN();
          break;
      }; break;
    case 1 :
      switch ( octet[1] ) {
        case 1 : for (int i = 0; i < stringLen; i++) {
            OBISVersion[i] = tempString[i];
          }
          DEBUG3_PRINT("StrLn: "); DEBUG3_PRINT(stringLen); DEBUG3_PRINT(" OBISV: "); DEBUG3_PRINT(tempString); DEBUG3_PRINT(" Cpy: ");  DEBUG3_PRINT(OBISVersion); DEBUG3_PRINTLN();
          break;
        case 0 :
          switch (octet [2]) {
            case  1:
              switch (octet [3]) {
                case  7 : activePowerQ1Q4 = longValue; DEBUG3_PRINT("activePowQ1Q4: "); DEBUG3_PRINTLN(); break; // todo unsigned
                case  8 : cumulativActiveIm = longValue; DEBUG3_PRINT("cumulativAcIm: ");  DEBUG3_PRINTLN(); break; // todo unsigned
              }; break;

            case  2:
              switch (octet [3]) {
                case  7 : activePowerQ2Q3 = longValue; DEBUG3_PRINT("activePowQ2Q3: ") ; DEBUG3_PRINTLN(); break; // todo unsigned
                case 8 : cumulativActiveEx = longValue; DEBUG3_PRINT("cumulativActEx: "); DEBUG3_PRINTLN(); break; // todo unsigned
              }; break;
            case  3:
              switch (octet [3]) {
                case  7 : reactivePowerQ1Q2 = longValue; DEBUG3_PRINT("reactivePowQ1Q2: ");  DEBUG3_PRINTLN(); break; // todo unsigned
                case  8 : cumulativReactiveIm = longValue;  DEBUG3_PRINT("cumulativReaIm: ");  DEBUG3_PRINTLN(); break; // todo unsigned
              }; break;
            case  4:
              switch (octet [4]) {
                case  7 : reactivePowerQ3Q4 = longValue; DEBUG3_PRINT("reactivePowerQ3Q4: ");  DEBUG3_PRINTLN(); break; // todo unsigned
                case  8 : cumulativReactiveEx = longValue;  DEBUG3_PRINT("cumulativReactiveEx: ");  DEBUG3_PRINTLN(); break; // todo unsigned
              }; break;
            case  31: currentL1 = intValue * pow(10, scaleValue); DEBUG3_PRINT("curL1: ");  DEBUG3_PRINTLN(); break; //
            case  51: currentL2 = intValue * pow(10, scaleValue); DEBUG3_PRINT("curL2: ");  DEBUG3_PRINTLN(); break; //
            case  71: currentL3 = intValue * pow(10, scaleValue); DEBUG3_PRINT("curL3: ");  DEBUG3_PRINTLN(); break; //

            case  32: phaseVL1 = intValue * pow(10, scaleValue); DEBUG3_PRINT("VL1: ");  DEBUG3_PRINTLN(); break; //
            case  52: phaseVL2 = intValue * pow(10, scaleValue); DEBUG3_PRINT("VL2: ");  DEBUG3_PRINTLN(); break; //
            case  72: phaseVL3 = intValue * pow(10, scaleValue); DEBUG3_PRINT("VL3: ");  DEBUG3_PRINTLN(); break; //
          }

      }
  }
}

void sendData() {
  /*
    MyMessage wattMsg(PoWCHILD_ID, V_WATT);
    MyMessage kwhMsg(PoWCHILD_ID, V_KWH);
    MyMessage reavtMsg(PoWCHILD_ID, V_VAR);  // reactive ?
    MyMessage ampMsg(PoWCHILD_ID, V_VA);  //
    MyMessage factorMsg(PoWCHILD_ID, V_POWER_FACTOR);
    MyMessage voltMsg(PoWCHILD_ID, V_VOLTAGE);
  */
#ifdef useMySensors
  if (activePowerQ1Q4 != 0) {
    send(wattMsg.set(activePowerQ1Q4));
  }
  if (cumulativActiveIm != 0) {
    send(kwhMsg.set((float)cumulativActiveIm / 100.0, 2));
  }
  if (currentL1 != 0) {
    send(ampMsg.set(currentL1, 2));
  }
  if (reactivePowerQ1Q2 != 0) {
    send(reavtMsg.set(reactivePowerQ1Q2));
  }
  if (phaseVL1 != 0) {
    send(voltMsg.set(phaseVL1, 2));
  }
  if ((reactivePowerQ1Q2 != 0) && (activePowerQ1Q4 != 0)) {
    send(factorMsg.set((float) reactivePowerQ1Q2 / (float)activePowerQ1Q4, 2));
  }
#endif
}


void printMsgContent() {
#ifdef MY_DEBUG1(x)
  Serial.println(); Serial.print("-----Record----"); Serial.println();
  Serial.print("MeterID:"); Serial.print(meterid); Serial.println();
  Serial.print("MeterType:"); Serial.print(meterType); Serial.println();
  Serial.print("OBISVersion:"); Serial.print(OBISVersion); Serial.println();
  Serial.print("date: ");
  //if (clockAndTime[1] != 0) {
  for (int i = 0; i < 12; i++) {
    Serial.print(clockAndTime[i], HEX); Serial.print("-");
    //  }
  }
  Serial.println();

  if (activePowerQ1Q4 != 0) {
    Serial.print("activePowerQ1Q4: "); Serial.print(activePowerQ1Q4, DEC); Serial.println();
  }

  if (activePowerQ2Q3 != 0) {
    Serial.print("activePowerQ2Q3: "); Serial.print(activePowerQ2Q3, DEC); Serial.println();
  }

  if (cumulativActiveIm != 0) {
    Serial.print("cumulativActiveIm: "); Serial.print(cumulativActiveIm, DEC); Serial.println();
  }
  if (cumulativActiveEx != 0) {
    Serial.print("cumulativActiveEx: "); Serial.print(cumulativActiveIm, DEC); Serial.println();
  }
  if (reactivePowerQ1Q2 != 0) {
    Serial.print("reactivePowerQ1Q2: "); Serial.print(reactivePowerQ1Q2, DEC); Serial.println();
  }
  if (reactivePowerQ3Q4 != 0) {
    Serial.print("reactivePowerQ3Q4: "); Serial.print(reactivePowerQ3Q4, DEC); Serial.println();
  }

  if (currentL1 != 0) {
    Serial.print("currentL1: "); Serial.print(currentL1, DEC); Serial.println();
  }
  if (currentL2 != 0) {
    Serial.print("currentL2: "); Serial.print(currentL2, DEC); Serial.println();
  }
  if (currentL3 != 0) {
    Serial.print("currentL3: "); Serial.print(currentL3, DEC); Serial.println();
  }

  if (phaseVL1 != 0) {
    Serial.print("phaseVL1: "); Serial.print(phaseVL1, DEC); Serial.println();
  }
  if (phaseVL2 != 0) {
    Serial.print("phaseVL2: "); Serial.print(phaseVL2, DEC); Serial.println();
  } if (phaseVL3 != 0) {
    Serial.print("phaseVL3: "); Serial.print(phaseVL3, DEC); Serial.println();
  }
  Serial.println(); Serial.print("-------END------"); Serial.println();
#endif
}

void nullTmpValues() {
  // null ut
  for (int i = 0; i < 6; i++) {
    octet[i] = 0;
  };
  for (int i = 0; i < 12; i++) {
    datatime[i] = 0;
  }
  for (int i = 0; i < stringLen; i++) {
    tempString[i] = 0;
  }
  stringLen = 0;
  OBIScode = 0;
  longValue = 0;
  unitValue = 0;
  intValue = 0;
  scaleValue = 0;
}

void nullValues() {
  activePowerQ1Q4 = 0;
  activePowerQ2Q3 = 0;
  reactivePowerQ1Q2 = 0;
  reactivePowerQ3Q4 = 0;
  currentL1 = 0;
  currentL2 = 0;
  currentL3 = 0;
  for (int i = 0; i < 8; i++) {
    clockAndTime[i] = 0;
  }
  phaseVL1 = 0;
  phaseVL2 = 0;
  phaseVL3 = 0;

  cumulativActiveIm = 0;
  cumulativActiveEx = 0;
  cumulativReactiveIm = 0;
  cumulativReactiveEx = 0;
  for (int i = 0; i < 20; i++) {
    meterid[i] = 0;
  }
  for (int i = 0; i < 20; i++) {
    meterType[i] = 0;
  }
  for (int i = 0; i < 20; i++) {
    OBISVersion[i] = 0;
  }
}

boolean testStartMark() {
  int c = readByte();
  if (c == startMark) {
    if (peekByte() == startMark) {// found end of previous buffer, this might happen if parcer is unasynron with the datastream
      int c = readByte();  // Read correct startmark
    }
    DEBUG2_PRINTLN(); DEBUG2_PRINTLN("StartMark Found ");
    return true;
  }
  return false;
}

boolean testEndMark() {
  int c = readByte();
  if (c == startMark) {
    return true;
  }
  return false;
}

boolean readPackegeLen() { //packagelen = 2 bytes ; “a”, 12 bit integer
  int c1 = readByte();
  if (c1 ^ 0xF0 == 0xa0) {
    DEBUG2_PRINT(" Len mrk:"); DEBUG2_PRINTHEX(c1);
    int c2 = readByte();
    bufferlen =  c2 | (c1 & 0x0F) << 8;
    if (bufferlen > buffMaxLen) {
      bufferlen = 2;  // Outside scope, try next
      DEBUG2_PRINT("Bufflen > max");
    }
    DEBUG2_PRINT("Len=" ); DEBUG2_PRINTDEC(bufferlen); DEBUG2_PRINT(" ");
    buffer [0] = c1;
    buffer [1] = c2;
    return true;
  }
  ERROR_PRINT("startlen not found");
  bufferlen = 0;
  return false;
}

int readByte() {
#ifdef useSoftSerial
  while (!mySerial.available()) {
  }
  int c = mySerial.read();
#else
  while (!Serial.available()) {
  }
  int c = Serial.read();
#endif
  DEBUG2_CODE(if (c < 16) Serial.print('0'));
  DEBUG2_PRINTHEX(c);
  return c;
}

int nextBuffByte() {
  if (bufferPos < buffMaxLen) {
    int c = buffer[bufferPos];
    DEBUG2_CODE(if (c < 16) Serial.print('0'));
    DEBUG2_PRINTHEX(c);
    bufferPos++;
    return c;
  }
}

int peekByte() {
#ifdef useSoftSerial
  while (!mySerial.available()) {
  }
  int c = mySerial.peek();
#else
  while (!Serial.available()) {
  }
  int c = Serial.peek();
#endif
  DEBUG2_CODE(if (c < 16) Serial.print('0'));
  DEBUG2_PRINT("Peek");
  DEBUG2_PRINTHEX(c);
  return c;
}

long readDouble() {
  long value  = 0;
  for (int i = 1; i <= 4; i++) {
    int c = nextBuffByte();
    value =  c | value  << 8;
  }
  longValue = value;
  DEBUG2_PRINTLN(); DEBUG2_PRINT("Double(32) = "); DEBUG2_PRINTDEC(longValue); DEBUG2_PRINTLN();
  return value;
}

int readLong() {
  int value  = 0;
  int high = nextBuffByte();
  int low = nextBuffByte();
  value =  low | high  << 8;
  intValue = value;
  DEBUG2_PRINTHEX(high); DEBUG2_PRINTHEX(low);
  DEBUG2_PRINTLN(); DEBUG2_PRINT(" Long(16) = " ); DEBUG2_PRINTDEC(intValue ); DEBUG2_PRINTLN();

  return value;
}

int readUnit() {
  int c = nextBuffByte();
  unitValue = c;
  DEBUG2_PRINTLN(); DEBUG2_PRINT("Enum =  " ); DEBUG2_PRINTDEC(c); DEBUG2_PRINTLN();
  return c;
}

int readStructureCrd() {
  int struktCrd = nextBuffByte();
  if (struktCrd < 22) {
    return struktCrd;
  }
  else {
    ERROR_PRINTLN(); ERROR_PRINT("Struct cardinality error" ); ERROR_PRINTHEX(struktCrd); ERROR_PRINTLN();

  }
}

boolean readDateTime() {
  int len = nextBuffByte();
  DEBUG2_PRINTLN(); DEBUG2_PRINT("DateLen=  " ); DEBUG2_PRINTDEC(len); DEBUG2_PRINTLN();
  if (len > 12) {
    ERROR_PRINTLN(); ERROR_PRINT("DateLen wrong: " ); ; ERROR_PRINTLN();

  }
  for (int i = 0; i < len; i++) {
    int c = nextBuffByte();
    datatime[i] = c;
  }
#ifdef MY_DEBUG2
  Serial.println("Octet: ");
  for (int i = 0; i < len; i++) {
    Serial.print(datatime[i], HEX ); Serial.print(".");
  }
#endif
  DEBUG2_PRINTLN();
}

boolean readOktet() {
  int len = nextBuffByte();
#ifdef MY_DEBUG2
  Serial.print("OctetLen: "); Serial.print(len, DEC ); DEBUG2_PRINTLN();
#endif
  if (len != 6) {

    ERROR_PRINT("OctetLen wrong: "); ERROR_PRINTLN();

  }
  // else {
  for (int i = 0; i < len; i++) {
    int c = nextBuffByte();
    octet[i] = c;
    //    }
  }
#ifdef MY_DEBUG2
  Serial.println("Octet: ");
  for (int i = 0; i < len; i++) {
    Serial.print(octet[i], DEC ); Serial.print(".");
  }
#endif
  DEBUG2_PRINTLN();
}

boolean myReadString() {
  int len = nextBuffByte();
  stringLen = len;
  for (int i = 0; i < len; i++) {
    int c = nextBuffByte();
    tempString[i] = c;
  }
  DEBUG2_PRINTLN("Str = "); DEBUG2_PRINT(tempString); DEBUG2_PRINTLN();
}

int8_t readScaler() {
  int8_t scale = nextBuffByte();
  DEBUG2_PRINTLN("Scal = "); DEBUG2_PRINTHEX(scale); DEBUG2_PRINTLN();
  scaleValue = scale;
  return scale;
  // scale value
}

int readArray() {
  int c = nextBuffByte();
  if (c == arrayMark) {
    int arrayCard = nextBuffByte();
    if (arrayCard > 0 && arrayCard < 20) {
      return arrayCard;
    }
    else {
      ERROR_PRINTLN(); ERROR_PRINT("Ary crd err"); ERROR_PRINTHEX(arrayCard); ERROR_PRINTLN();
    }
  }
  else {
    ERROR_PRINTLN(); ERROR_PRINT("Arr NF"); ERROR_PRINTHEX(c); ERROR_PRINTLN();

    return 0;
  }
}


