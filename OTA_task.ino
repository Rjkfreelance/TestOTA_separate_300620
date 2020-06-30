void taskOTA(void*){

  VOID SETUP(){      
     
   }

  VOID LOOP(){  
  switch (state)
  {
    case Runnning_e:
     if (!client.connected()) {
        while (!client.connected()) {
         if (client.connect(clientId, mqttUser, mqttPassword)) {
             Serial.println("Mqtt....connected");
             /* subscribe topic */
              client.subscribe(stopic,Qos);
               client.subscribe(gtopic,Qos);
                client.subscribe(ctopic,Qos);
             }
          }
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
        DELAY(100);
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

 DELAY(1000);
  
}
