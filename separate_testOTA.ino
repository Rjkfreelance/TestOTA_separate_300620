#include <WiFi.h>
#include <PubSubClient.h> 
#include <HttpFOTA.h>
#include <string.h>
#include <stdio.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"

/*--------------Config Variable---------------------*/
const char* wifi_pwd;
const char* wifi_ssid;
const char* mqtt_server;
int mqttPort;
const char* mqttUser;
const char* mqttPassword;
const char* clientId;
int otatimeout; // OTA Timeout limit 30 sec
const char* sendtopic; // Machine send data Topic
const char* gtopic; //OTA Group Topic 
const char* ctopic; //OTA Sub Companny Topic
const char* stopic; //OTA Self Machine Topic
const char* ackota; //OTA Acknowledge use for Machine confirm received OTA
const char* getconf; // //Topic of this machine subscribe (or listen) for receive command from web socket command getcf(get config)
const char* sendconf; // Topic for Machine send config back to(publish to) web server (use web socket)
const char* dbreply;//Topic for check db active  Server Reply OK if Insert data already  ADD BY RJK 
String sendperiod;//Config send every msec (Additional 18/06/2020)


String eachline;// String  Object receiving each line in file conf.txt

char* certssl; // SSL Certification for download firmware OTA Load from certi.txt
String  Certs = "";// String Object for concatination certification from file certi.txt

/* String Object array for keep config each line in file conf.txt in SD Card */
String Line[17];
File iotfmx; //create object root from Class File use SD Card use write new config to SD Card
File root; //create object root from Class File use SD Card read from SD Card and assign to config variable



#define Qos  1 //Quality of Service

unsigned long tnow = 0; //Init time for plus current time
unsigned long  startota = 0; //Initial time for counter 
int ota_t; //time difference
int prog; //percent download progress
  
WiFiClient FMXClient;
PubSubClient client(FMXClient);

typedef enum {
    Runnning_e = 0x01,
   Fota_e  
}SysState;

/*-----------------Firmware Source Download------------------------*/
char url[100];//Url firmware download 
char md5_1[50];// md5 checksum firmware filename .bin

SysState state = Runnning_e;  //Create an instance state


void sdbegin()
{
  if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    return;
  } else {
    Serial.println("SD Card OK");
  }

}
void assignConfig(fs::FS &fs, const char* path) {
  File file = fs.open(path);
  if (!file) {
    Serial.println("Failed to open directory");
    return;
  }
  
  Serial.print("Reading file: ");
  Serial.println(path);
    int n =0;
    while (file.available()) {
     eachline = file.readStringUntil('\n');    
     int posi = eachline.indexOf(':');
     String val = eachline.substring(posi+1);
      Serial.println(val);
      Line[n] = val;
      Line[n].trim();
      n++;
    }
   wifi_ssid = (const char*)Line[0].c_str();
   
   wifi_pwd = (const char*)Line[1].c_str();
  
   mqtt_server = (const char*)Line[2].c_str();
 
   mqttPort = Line[3].toInt();
   
   //Serial.println(mqtt_server);//debug ok
   //Serial.println(mqttPort);//debug ok

   mqttUser = (const char*)Line[4].c_str();

   mqttPassword = (const char*)Line[5].c_str();
   
   clientId = (const char*)Line[6].c_str();
 
   otatimeout = Line[7].toInt();
   sendtopic = (const char*)Line[8].c_str();
  
   gtopic = (const char*)Line[9].c_str();
   ctopic = (const char*)Line[10].c_str();
   stopic = (const char*)Line[11].c_str();//otamachine 
   dbreply =  (const char*)Line[12].c_str(); // add by rjk
   getconf = (const char*)Line[13].c_str();
   //sendperiod = (const char*)Line[14].c_str();// add by rjk
   sendperiod = Line[14];
   ackota = (const char*)Line[15].c_str();
   sendconf = (const char*)Line[16].c_str();
   
}

String readcert(fs::FS &fs, const char* path){
 File fr = fs.open(path);
  String SL = "";
  while(fr.available()){
    SL += fr.readStringUntil('\n');
    }
   fr.close();
  return SL;
}

void progress(DlState state, int percent){ //Show % Progress Download Firmware from Server
  Serial.printf("state = %d - percent = %d %c\n", state, percent,'%');//Print format show % progress
     prog = percent; 
     ota_t = millis() - startota;
     chk_ota_timeout(ota_t);//Call function for check timeout
 }
 
/*  Refer to errorCallback method in HttpFOTA.h   */
void error(char *message){ //Show Error Message During OTA
  printf("%s\n", message);
}

/*  Refer to startDownloadCallback method in HttpFOTA.h  */
void startDl(void){ // Start Download Firmware Function
  startota = millis();
}

void endDl(void){ //Show Time to OTA Finish Function 
  ota_t = millis() - startota;
  float otafinish = ota_t/1000.0;  //Sec
  Serial.print(String(ota_t) + " ms ");
  Serial.print(otafinish,2);
  Serial.println(" Sec.");
}

void chk_ota_timeout(unsigned long tm){ //Check TimeOut OTA Function 
  if((tm >= otatimeout)&&(prog < 100)){
   Serial.printf(" Time out! %d\n",tm);
   delay(50);
   ESP.restart();
  }
}

void function_ota()
{
  /*-------------------FOR OTA--------------------------*/
  switch (state)
  {
    case Runnning_e:

      if (!client.connected()) {
        mqttconnect();
      }

      client.loop();
      break;
    case Fota_e:
      DlInfo info;
      info.url = url;
      // info.caCert = NULL;//if only use http then remember to set this to NULL
      info.caCert =  certssl; //SSL Cert iotfmx.com (secure server load from certi.txt)
      info.md5 = md5_1; // info.md5 is argument of setMD5(const char * expected_md5) in Update.h
      info.startDownloadCallback =  startDl;
      info.endDownloadCallback =    endDl;
      info.progressCallback  = progress;
      info.errorCallback     = error;
      int result = httpFOTA.start(info); //OTA Method
      if(result < 0){ // Check error return from class HttpFOTA
        delay(100);
        ESP.restart(); 
      }
      /*
      if(result == 1){
       String DT =  DateTimeNOW();
               DT += " OTA OK";
        client.publish(ackota,DT.c_str(),Qos);
        delay(1000);
        ESP.restart();  
      }
      */
     break;
  }
}


void OTACallback(char *topic, byte *payload, unsigned int length){
  Serial.println(topic);//Print topic received
  Serial.println((char*)payload);//print payload (or message) in topic received
  if((strncmp(gtopic, topic, strlen(gtopic)) == 0)||(strncmp(ctopic, topic, strlen(ctopic)) == 0)||(strncmp(stopic, topic, strlen(stopic)) == 0)){
    memset(url, 0, 100);
    memset(md5_1, 0, 50);
    char *tmp = strstr((char *)payload, "url:");//Query url: in payload(message received) and assign to pointer tmp
    char *tmp1 = strstr((char *)payload, ",");//Query , in payload(message received) and assign to pointer tmp1
    memcpy(url, tmp+strlen("url:"), tmp1-(tmp+strlen("url:")));
    
    char *tmp2 = strstr((char *)payload, "md5:");//Query md5: in payload(message received) and assign to pointer tmp2
    memcpy(md5_1, tmp2+strlen("md5:"), length-(tmp2+strlen("md5:")-(char *)&payload[0]));

    Serial.printf("started fota url: %s\n", url);
    Serial.printf("started fota md5: %s\n", md5_1);
    client.publish(ackota,stopic);// Publish message OTA TOPIC if acknowledge OTA backward to web server 
    state = Fota_e; //Start state Firmware OTA
   }
}

int mqttconnect() {
  /* Loop until reconnected */
  while (!client.connected()) {
    //while(1){
    /* connect now */
    if (client.connect(clientId, mqttUser, mqttPassword)) {
      // if (client.connect(clientId,mqttUser,mqttPassword)) {
      Serial.println("Mqtt....connected");
      
      /* subscribe topic */
       client.subscribe(stopic,Qos);
       client.subscribe(gtopic,Qos);
        client.subscribe(ctopic,Qos);
    }
  }
}

void wifi_setup()
{ 
  
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid,wifi_pwd); //assign wifi ssid , pass

  while (WiFi.status() != WL_CONNECTED) {//Loop is not connect until connected exit loop
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("Connect ok");
}

void setup() {
  Serial.begin(115200);
  sdbegin();
  assignConfig(SD,"/conf.txt");
  Serial.println(F("Connected SD Card ok."));
  Serial.println(F("Load config from SD card file:conf.txt"));
   certssl = (char*)readcert(SD,"/certi.txt").c_str(); //Load certificate and convert to char* datatype
    wifi_setup();
     client.setServer(mqtt_server,mqttPort); 
     client.setCallback(OTACallback);
      mqttconnect();
}

void loop() {
  
    function_ota();
    
    Serial.println("Test...waiting OTA");
    delay(250);
}
