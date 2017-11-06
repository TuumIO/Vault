#include <SPI.h>
#include <WiFi101.h>
#include <WiFiUdp.h>
#include <WiFiClient.h>
#include <RTCZero.h>
#include <FlashStorage.h>
#include <LiquidCrystal.h>
#include <Time.h> //funciones Time
//#include "ToolFunctions.h"
//#include <TelegramBot.h>

char apssid[] = "Vault";
int status = WL_IDLE_STATUS;
WiFiServer server(80);

int ledState = LOW;
const int ledPin =  6;                      // MKR1000 Led Pin es 6
//const int buttonPin = 4;
unsigned int localPort = 2390;              // local port to listen on
char packetBuffer[255];                     //buffer to hold incoming packet
int netCounter = 0;                         //Contador conexion red
//int buttonState = 0;                        // current state of the button
volatile byte lastLedState = LOW;                     // previous state of the button
String mensajeUdf;
String ctrlUdf = "VAULT";
char  ReplyBuffer[] = "Recibido";
char sysStatus[8];                      //Status sistema

int numRuns = 0;      // Execution count, so this doesn't run forever
int numRuns2 = 0;   // Execution count, so this doesn't run forever

IPAddress timeServer(129, 6, 15, 28);   // time.nist.gov NTP server
const int NTP_PACKET_SIZE = 48;         // NTP time stamp is in the first 48 bytes of the message
byte pBuffernpt[ NTP_PACKET_SIZE];      //buffer to hold incoming and outgoing packets

byte seconds = 0;
byte minutes = 0;
byte hours = 0;

byte days = 0;
byte months = 0;
byte years = 0;

int netIndex, passIndex;
String network, password;

boolean needCredentials = true;
boolean needWiFi = false;
boolean connectedWiFi = false;
boolean needTime = false;

const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

//const char* BotToken = "448481229:AAHd6H7RsFq3t8R1t3w10L8c2RLNBmxXttQ";

WiFiSSLClient client;  
//TelegramBot bot(BotToken,client);

typedef struct {
  boolean valid;
  char ssid[100];
  char pass[100];
} netconf;

FlashStorage(my_flash_store, netconf);
netconf netUser;

RTCZero rtc;

WiFiUDP Udp;

void setup() {
  
  lcd.display();
  lcd.begin(16, 2);
  lcd.print("Hola Emilio!");
  netUser = my_flash_store.read();
  //pinMode(buttonPin, INPUT);
  pinMode(ledPin, OUTPUT);
  //attachInterrupt(digitalPinToInterrupt(4), pButton, RISING);
  /*
  Serial.begin(9600);
    while (!Serial) {
      ;                                         // wait for serial port to connect. Needed for native USB port only
    }
  */  
  if (netUser.valid == true){
    needCredentials = false;
    needWiFi = true;
    network = String(netUser.ssid);
    password = String(netUser.pass);
    //Serial.println(network);
    //Serial.println(password);
  }
  else {  
    lcd.clear();
    WiFi.beginAP(apssid);
    delay(2000);
    lcd.print("Modo Hotspot");
    printAPStatus();
    server.begin();
  }
}
void loop() {
  digitalWrite(ledPin, lastLedState);
  if (needCredentials) {
    getCredentials(); 
  }
  if (needWiFi) {
    getWiFi(); 
  }
  if (needTime) {
    timeNpt();
    rtc.begin();
    rtc.setHours(hours);
    rtc.setMinutes(minutes);
    rtc.setSeconds(seconds);
    // aca se debe encontrar las tunciones de Time.h?
    rtc.setDay(days);
    rtc.setMonth(months);
    rtc.setYear(years);
    needTime = false;
    pFecha();
  }
  if (connectedWiFi) {
        txUdp();
        //telegram();
  }
}
void getWiFi () {
    while (WiFi.status() != WL_CONNECTED && netCounter < 5) {
        lcd.clear();
        lcd.print("SSID:");
        lcd.setCursor(6, 0);
        lcd.print(network);
        WiFi.begin(network, password);
        netCounter ++;
        lcd.setCursor(0, 1);
        lcd.print("Intento #:");
        lcd.setCursor(12, 1);
        lcd.print(netCounter);
        delay(1000);
    }
    if (netCounter == 5) {
        lcd.clear();
        lcd.print("Error de Conexion!!");
        lcd.setCursor(0, 1);
        lcd.print("Modo Hotspot!");
        netCounter = 0;
        needCredentials = true;
        needWiFi = false;
    }
    else {
        lcd.clear();
        lcd.print("Conectado :)");
        needWiFi = false;
        connectedWiFi = true;
        if (netUser.valid == false){
          netUser.valid = true;    //Escribe en la Flash cuando hace la conf inicial
          int lenA = network.length() + 1;
          int lenB = password.length() + 1;
          network.toCharArray(netUser.ssid, lenA);
          password.toCharArray(netUser.pass, lenB);
          my_flash_store.write(netUser);
        }
        printWifiData();
        needTime = true;
        Udp.begin(localPort);
        lcd.noDisplay();
    }
}

void txUdp() {
  int packetSize = Udp.parsePacket();
  if (packetSize)
  {
    int len = Udp.read(packetBuffer, 255);
    mensajeUdf = packetBuffer;
    if (mensajeUdf.equals(ctrlUdf)) {
      packetSize = 0;
      if (lastLedState == HIGH) {
        digitalWrite(ledPin, LOW);
        lastLedState = LOW;
      }
      else {
        digitalWrite(ledPin, HIGH);
        lastLedState = HIGH;
      }
      pFecha();
      thetimeis();      
    }
    for(int i=0;i<255;i++) packetBuffer[i] = 0;
    mensajeUdf = "";
    Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
    Udp.write(ReplyBuffer);
    Udp.endPacket();
  }
}
void printWifiData() {
  IPAddress ip = WiFi.localIP();
  lcd.setCursor(0, 1);
  lcd.print(ip);
}
void timeNpt() {
  sendNTPpacket(timeServer); // send an NTP packet to a time server
  // wait to see if a reply is available
  delay(1000);
  if ( Udp.parsePacket() ) {
    //Serial.println("packet received");
    // We've received a packet, read the data from it
    Udp.read(pBuffernpt, NTP_PACKET_SIZE); // read the packet into the buffer

    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:

    unsigned long highWord = word(pBuffernpt[40], pBuffernpt[41]);
    unsigned long lowWord = word(pBuffernpt[42], pBuffernpt[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    //Serial.print("Seconds since Jan 1 1900 = " );
    //Serial.println(secsSince1900);

    // now convert NTP time into everyday time:
    // Serial.print("Unix time = ");
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    // subtract seventy years:
    unsigned long epoch = secsSince1900 - seventyYears;
    // print Unix time:
    // Serial.println(epoch);
    setTime(epoch); // Set time de las funciones Time.h
    hours = hour();
    minutes = minute();
    seconds = second();
    days = day();
    months = month();
    years = year();
    /*
    // print the hour, minute and second:
    //Serial.print("La hora UTC es: ");       // UTC is the time at Greenwich Meridian (GMT)
    hours = (epoch  % 86400L) / 3600;
    //String shours = String(hours, DEC);
    //Serial.print(hours); // print the hour (86400 equals secs per day)
    //Serial.print(':');
    if ( ((epoch % 3600) / 60) < 10 ) {
      // In the first 10 minutes of each hour, we'll want a leading '0'
      //Serial.print('0');
    }
    minutes = (epoch  % 3600) / 60;
    //String sminutes = String(minutes, DEC);
    //Serial.print(minutes); // print the minute (3600 equals secs per minute)
    //Serial.print(':');
    if ( (epoch % 60) < 10 ) {
      // In the first 10 seconds of each minute, we'll want a leading '0'
      //Serial.print('0');
    }
    seconds = epoch % 60;
    //String sseconds = String(seconds, DEC);
    //Serial.println(seconds); // print the second
    //String ttotal = String(shours + ":" + sminutes + ":" + sseconds);
    */
}
  // wait ten seconds before asking for the time again
  //delay(10000);
}

// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress& address)
{
  //Serial.println("1");
  // set all bytes in the buffer to 0
  memset(pBuffernpt, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  //Serial.println("2");
  pBuffernpt[0] = 0b11100011;   // LI, Version, Mode
  pBuffernpt[1] = 0;     // Stratum, or type of clock
  pBuffernpt[2] = 6;     // Polling Interval
  pBuffernpt[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  pBuffernpt[12]  = 49;
  pBuffernpt[13]  = 0x4E;
  pBuffernpt[14]  = 49;
  pBuffernpt[15]  = 52;

  //Serial.println("3");

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  //Serial.println("4");
  Udp.write(pBuffernpt, NTP_PACKET_SIZE);
  //Serial.println("5");
  Udp.endPacket();
  //Serial.println("6");
 }

void printAPStatus() {
  //Serial.print("SSID: ");
  //Serial.println(WiFi.SSID());
  IPAddress ip = WiFi.localIP();
  //Serial.print("IP Address: ");
  //Serial.println(ip);
  //Serial.print("Para conectarte, entra a la siguiente direccion http://");
  //Serial.println(ip);
  lcd.setCursor(0, 1);
  lcd.print(ip);
}

void getCredentials() {
    WiFiClient client = server.available();
    if (client) {
        String currentLine = "";
        while (client.connected()) {
            if (client.available()) {
                char c = client.read();
                //Serial.print(c);
                if (c == '\n') {
                    if (currentLine.length() == 0) {
                        //send HTML to client
                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-type:text/html");
                        client.println();
                        client.println("<html>");
                        client.println("<body>");
                        client.println("<form method=get>");
                        client.print("NETWORK: ");
                        client.print("<input type=\"text\" name=\"network\"/><br>");
                        client.print("PASSWORD: ");
                        client.print("<input type = \"text\" name=\"password\"/><br>");
                        client.print("<input type=submit value=Submit></form>");
                        client.println("</body>");
                        client.println("</html>"); 
                        client.println(); 
                        break;
                    }
                    else {
                        //check for data
                        if (currentLine.indexOf("?network=") != -1) {
                            netIndex = currentLine.indexOf("?network=");
                        }
                        if (currentLine.indexOf("&password=") != -1) {
                            passIndex = currentLine.indexOf("&password=");
                            network = currentLine.substring(netIndex+9, passIndex);
                            password = currentLine.substring(passIndex+10, currentLine.indexOf(" HTTP/1.1"));
                            client.stop();
                            WiFi.end();
                            needCredentials = false;
                            needWiFi = true;  
                        }
                        currentLine = "";
                    }
            }
            else if (c != '\r') {
                currentLine +=c;
            }
        }
    }
    client.stop();
  }
}
void pFecha() {
  int evDay = rtc.getDay();
  int evMonth = rtc.getMonth();
  int evYear = rtc.getYear();
  String sDay = String(evDay, DEC);
  String sMonth = String(evMonth, DEC);
  String sYear = String(evYear, DEC);
  String tfecha = sDay + "-" + sMonth + "-" + sYear;
  lcd.display();
  lcd.clear();
  lcd.print("Hoy es:");
  lcd.setCursor(0, 1);
  lcd.print(tfecha);
  delay(2000);
  lcd.clear();
}
void thetimeis(){
  int evHours = rtc.getHours();
  int evMinutes = rtc.getMinutes();
  int evSeconds = rtc.getSeconds();
  String shours = String(evHours, DEC);
  String sminutes = String(evMinutes, DEC);
  String sseconds = String(evSeconds, DEC);
  String ttime = shours + ":" + sminutes + ":" + sseconds;
  lcd.display();
  lcd.clear();
  lcd.print("Hora:");
  lcd.setCursor(0, 1);
  lcd.print(ttime);
  delay(2000);
  lcd.clear();
}
