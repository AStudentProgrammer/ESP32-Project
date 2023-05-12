#include <Arduino.h>
#include <WiFi.h>
#include <Keypad.h>
#include <safety_rig.h>

// USER SETABLE VALUES HERE---------------------------------------------------
char smBuf[256];
//Set speeds
float actualSpeed = 0;

const int MODETIMESTART = 1000;
int tempvLineCurrent;
int stopRig;

boolean printedOnce = false;
// END USER SETABLE VALUES-----------------------------------------------------

TaskHandle_t Task1;

void SendReceiveData(void * pvParameters){
    for(;;){
        if(WiFi.status() == WL_CONNECTED){
            vTaskDelay(10000 / portTICK_PERIOD_MS);
            continue;
        }
        WiFi.mode(WIFI_STA);
        WiFi.begin(WIFI_SSID, WIFI_PASS);
        
        WiFi.onEvent(WiFiStationDisconnected, SYSTEM_EVENT_STA_DISCONNECTED);
        unsigned long startAttemptTime = millis();

        // Keep looping while we're not connected and haven't reached the timeout
        while (WiFi.status() != WL_CONNECTED && 
        millis() - startAttemptTime < WIFI_TIMEOUT_MS){}

    }
}

void setup(){
    Serial.begin(baud_rate);
    lcd.begin();
    keypad.addEventListener(keypadEvent); // Add an event listener for this keypad
    
    xTaskCreatePinnedToCore(
             SendReceiveData, /* Task function. */
             "Send_Receive",   /* name of task. */
             10000,     /* Stack size of task */
             NULL,      /* parameter of the task */
             1,         /* priority of the task */
             &Task1,    /* Task handle to keep track of created task */
             0);        /* pin task to core 0 */

    // connect to vline

    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    WiFi.onEvent(WiFiStationDisconnected, SYSTEM_EVENT_STA_DISCONNECTED);
    Serial.println("Setup complete!");
    coldStart = true;

}

void loop(){
// Runs motor control commands on Core 1
  if(coldStart){
    master.connect(server, port);
    Serial.println("Binding socket....");
    coldStart =false;
    delay(2500);
  }

  if(master.connected()){
    connected = true;
    // listen to vline msg/
    ReadVlineInfo();
    // send stop rig signal

  }else{
    master.connect(server, port);
    Serial.println("Rebinding socket....");
    delay(2500);
  }

  char key = keypad.getKey();
  printCurrentMode();   
  numberOfRevs();

  //Reset motor speed to zero for 3 seconds
  //before starting new command
  if (modeTime < MODETIMESTART) {
      motorControl(6);
      if (actualSpeed < desiredSpeed())
          actualSpeed += 2.0f;
      else if (actualSpeed > desiredSpeed())
          actualSpeed -= 2.0f;
      ser.set(prop.ctrl_velocity_, actualSpeed);
      fromStart = true;
      actualSpeedReached = false;
      modeTime++;
      return;
  }

  /*Check if motor slows down to much from normal 
operating function and stop motor if it does*/

  if (stopWheel || stopRig == 1) {
      ser.set(prop.ctrl_coast_);
      if (!printedOnce) {
          if (stopWheel) {
              Serial.print("Exceeded revoluitons");
          } else {
              Serial.print("Slow speed stop");
          }
          printedOnce = true;
      }
      return;
  }

  //Set mode to perform respective tasks
  if (mode == 1) {
      relayEnduranceTest();
      ser.set(prop.ctrl_velocity_, actualSpeed);
  } else if (mode == 2) {
      newHoveringTest(2);
      ser.set(prop.ctrl_velocity_, actualSpeed);
  } else if (mode == 8) {
      endOfReelTest();
      ser.set(prop.ctrl_velocity_, actualSpeed);
  } else if (mode == 9) {
      if (revolutions <= 5) {
          mode = 0;
          return;
      }
      motorControl(2);
      ser.set(prop.ctrl_velocity_, actualSpeed);
  } else {
      ser.set(prop.ctrl_coast_);
  }
  //Make the motor accelerate or deccelrate smoothly
  if (actualSpeed < desiredSpeed())
      actualSpeed += 2.0f;
  else if (actualSpeed > desiredSpeed())
      actualSpeed -= 2.0f;
  else
      actualSpeedReached = true;

  //Observe if motor slow down for too long
  velocityInfo();

#ifdef enableSerial
  serialPrint();
#endif

  // Limit the acceleration by updating at a limited rate
  delay(25);
}