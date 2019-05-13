# Norwegian AMS power meter sensor/adapter for MySensors and controlers like Domoticz
  
  Implemetation of sensornode for Mysensor, a parser that read OBIS codes and meter data from the HAN port of a Norwegian Aidon power meter and a minimalistic ardino like prosessor to run it.
  
  Tested on a Hafslund metter and on Eidsiva
  
  The parser decode a Mbus datastream according to the Norwegian HANcode specification - OBIS codes.
  This adapter is relativ generic, independent of the order and content of each message and record.
  ### Specification:
  Ref https://www.nek.no/wp-content/uploads/2018/11/Aidon-HAN-Interface-Description-v10A-ID-34331.pdf
  and EXCERPT DLMS UA Blue Book: COSEM interface classes and OBIS identification system, EXCERPT DLMS UA 1000-1 Ed. 12.0
  
  Hardware is an arduino that reads a serial data stream form a Mbus to ttl adapter.  
  The MBus converter is connected to the HAN port of the power meter.
  The Adapter is tested with Hafslund Aidon meter, a arduino my sensor node and a Mbus adapter:
  https://www.ebay.com/itm/TSS721A-Breakout-Module-with-Isolation/113359924361?hash=item1a64c72c89:g:gS0AAOSwF31b5paL:rk:4:pf:0

  The arduino MySensor node is connected to a rasperry Domoticz controller via a 2.4 Mhz mesh network. Ref mySensor.org
  
  Keywords: AMS powermeter, HAN port, OBIS, Mbus, COSEM, Domotizc, mysensors
  

 ### Notes:
  Software serial can be used to test the parser,
  but on a 3.3V on 8Mhz this might be to slow when parsing the one hour message, use rx tx. 
  
  The 8mhz arduino is of this type:
  https://forum.mysensors.org/topic/2067/my-slim-2aa-battery-node
  
  This Arduno can probably be powered from the Mbus interface. This is not tested.
  Currently the card is powered with 3.3V via a step-down regulator from 5 v, it consumed 35ma, including loss in the regulator.

  The mysensors data is sent in senddata().

  This parser is a top down recursive parser driven by the grammar of the message,
  specified in the reference above.  The basic grammar is:

  Hdlcpackage = startmark, packagelen, frameheader, payload, FCC, endmark<br/>
  startmark = endmark = “7E”<br/>
  packagelen = 4 byte ; “A”, 12 bit integer<br/>
  frameheader = 9 byte ; Not checked, ends with a crc<br/>
  payload = dataheader, array, register*<br/>
  dataheader = “0f 40 00 00 00 00”  ; might not be fixed<br/>
  array = «01», dataLen<br/>
  dataLen = 1 byte; Int<br/>
  register = structure, structureContent;<br/>
  structureContent = octetString, OBIScodeandContent  ; se code for details<br/>

  ## Example:

  Header   a0 2a 41 08 83 13 04 13 e6 e7 00 : FrameType: 10 FrameLength: 42<br/>
  DataHeader 0f 40 00 00 00 00<br/>
  Type & Len 01 01<br/>
  02 03  : Structur – 3  <br/>
  09 06  : ocet-string len 6<br/>
  01 00 01 07 00 ff : OBIS kode  '1.0.1.7.0.255<br/>
  06  : double-long-unsigned   (32bit)<br/>
  00 00 06 44  : dec 1604<br/>
  02 02 : Structur – 2 <br/>
  0f  Int8 <br/>
  00 : 0 <br/>
  16  : enum  <br/>
  1b : Watt active power<br/>
  6f 92  CRC<br/>

  ----
  TODO: CRC is not checked,
  
  test power form the Mbus
  
  restructure the parser to a c++ class
  
  There is a isue  with parcing the last parts of the "one hour message", but it does not effect the data.
  
  ## Example on Hour record, partly decoded
7E<br/>
A13E410883137F8EE6E700<br/>  //length 318
0F4000000000<br/>
010E  // Array 14

1. 0202 0906 0101000281FF 0A0B 4149444F4E5F5630303031  //OBIS list version<br/>
2. 0202 0906 0000600100FF 0A10 47333539393335383933933353836300 //måler nr  Nr er ofuskert<br/>
3. 2020 9060 000600107FF 0A04 363531350  // måler type<br/>
4. 2030 9060 100010700FF 06 0000042E 0202 0F00 161B  //activePowerQ1Q4<br/>
5. 0203 0906 0100020700FF 06 00000000 0202 0F00 161B <br/>
6. 0203 0906 0100030700FF 06 0000007C 0202 0F00 161D<br/>
7. 0203 0906 0100040700FF 06 00000000 0202 0F00 161D<br/>
8. 0203 0906 01001F0700FF 10 002F 0202 0FFF 1621 <br/>
9. 0203 0906 0100200700FF 12 0941 0202 0FFF 1623<br/>
10. 0202 0906 0000010000FF 090C 07E3021201130000FF000000   //Time and date<br/>
11. 0203 0906 0100010800FF 06 0020C2BC 0202 0F01 161E<br/>
12. 0203 0906 0100020800FF 06 00000000 0202 0F01 161E <br/>
13. 0203 0906 0100030800FF 06 000216B8 0202 0F01 1620<br/>
14. 0203 0906 0100040800FF 06 0000FB60 0202 0F01 1620<br/>
15. 166C 7E

#### data: 
MeterID:123456789012<br/>
MeterType:6515<br/>
OBISVersion:AIDON_V0001<br/>
date: 0-0-0-0-0-0-0-0-36-35-31-35-<br/>
activePowerQ1Q4: 1872<br/>
cumulativActiveIm: 2203350<br/>
reactivePowerQ1Q2: 294<br/>
currentL1: 8.1000003814<br/>
phaseVL1: 237.5000000000<br/>


## Hardware

![bilde1](bilder/20190307_202741.jpg)
![bilde2](bilder//20190307_203608.jpg)
![bilde3](bilder//20190307_203939.jpg)

## Power meter in Domoticz 

![bilde1](bilder/Screenshot_20190305-125031_Chrome.jpg)
![bilde2](bilder/Screenshot_20190305-125121_Chrome.jpg)
![bilde3](bilder/Screenshot_20190307-204804_Chrome.jpg)

THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.




