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
char packetBufudp[255];                     //buffer to hold incoming packet
int netCounter = 0;                         //Contador conexion red
//int buttonState = 0;                        // current state of the button
volatile byte lastLedState = LOW;                     // previous state of the button
String mensajeUdf;
String ctrlUdf = "VAULT";
char  ReplyBuffer[] = "Recibido";
char sysStatus[8];                      //Status sistema

int numRuns = 0;      // Execution count, so this doesn't run forever
int numRuns2 = 0;   // Execution count, so this doesn't run forever

IPAddress timeServer(132, 163, 4, 101); 

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

const int timeZone = 1;     // Central European Time

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

time_t prevDisplay = 0; // when the digital clock was displayed

void loop() {
  digitalWrite(ledPin, lastLedState);
  if (needCredentials) {
    getCredentials(); 
  }
  if (needWiFi) {
    getWiFi(); 
  }
  if (needTime) {
    setSyncProvider(getNtpTime);
    prevDisplay = now();
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
    int len = Udp.read(packetBufudp, 255);
    mensajeUdf = packetBufudp;
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
    for(int i=0;i<255;i++) packetBufudp[i] = 0;
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
time_t getNtpTime()
{
  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  //Serial.println("Transmit NTP Request");
  sendNTPpacket(timeServer);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      //Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  //Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:                 
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
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
  int evDay = day();
  int evMonth = month();
  int evYear = year();
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
  int evHours = hour();
  int evMinutes = minute();
  int evSeconds = second();
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

