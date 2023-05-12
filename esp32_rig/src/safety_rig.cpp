#include <Arduino.h>
#include <WiFi.h>
#include <Keypad.h>
#include <HardwareSerial.h>
#include <safety_rig.h>
// #include <Wire.h>

#define RXD2 16
#define TXD2 17

// USER SETABLE VALUES HERE---------------------------------------------------
char smBuf[256];
// Set speeds
float actualSpeed = 0;

const int MODETIMESTART = 100;
int tempvLineCurrent;
int stopRig;

TaskHandle_t Task1;
TaskHandle_t Task2;
TaskHandle_t Task3;

QueueHandle_t xQueue;
static const int queueSize = 10;

typedef struct
{
  int motor_mode; int motor_revolutions;
} Data_t;

// typedef struct
// {
//   int motor_mode; int motor_revolutions;
// } Vline_t;

boolean printedOnce = false;
// END USER SETABLE VALUES-----------------------------------------------------

void motor(){
    numberOfRevs();
    // Reset motor speed to zero for 3 seconds
    // before starting new command
    if (modeTime < MODETIMESTART)
    {
        // Serial.println("Changing mode!");
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
    
    if (stopWheel || stopRig == 1)
    {
        ser.set(prop.ctrl_coast_);
        if (!printedOnce)
        {
            if (stopWheel)
            {
                Serial.print("Exceeded revoluitons");
            }
            else
            {
                Serial.print("Slow speed stop");
            }
            printedOnce = true;
        }
        return;
    }

    // Set mode to perform respective tasks
    if (mode == 1)
    {
        // end of reel
        endOfReelTest();
        ser.set(prop.ctrl_velocity_, actualSpeed);
    }
    else if (mode == 2)
    {   
        // relay test
        relayEnduranceTest();
        ser.set(prop.ctrl_velocity_, actualSpeed);
    }
    else if (mode == 3)
    {
        // hovering test
        newHoveringTest(5);
        ser.set(prop.ctrl_velocity_, actualSpeed);
    }
    else if (mode == 7)
    {
        // troubleshooting (Out)
        if (revolutions >= 140)
        {
            mode = 0;
            return;
        }
        motorControl(8);
        ser.set(prop.ctrl_velocity_, actualSpeed);
    }
    else if (mode == 8)
    {
        // troubleshooting (In)
        if (revolutions <= 5)
        {
            mode = 0;
            return;
        }
        motorControl(7);
        ser.set(prop.ctrl_velocity_, actualSpeed);
    }
    else if (mode == 9)
    {
        // Back to start 
        if (revolutions <= 20)
        {
            mode = 0;
            return;
        }
        motorControl_2(-1.5);
        ser.set(prop.ctrl_velocity_, actualSpeed);
    }
    else if (mode == 0)
    {
        stop();
        ser.set(prop.ctrl_velocity_, actualSpeed);
    }
    else
    {
        ser.set(prop.ctrl_coast_);
    }
    // Make the motor accelerate or deccelrate smoothly
    if (actualSpeed < desiredSpeed())
        actualSpeed += 2.0f;
    else if (actualSpeed > desiredSpeed())
        actualSpeed -= 2.0f;
    else
        actualSpeedReached = true;

    // Observe if motor slow down for too long
    velocityInfo();

#ifdef enableSerial
    serialPrint();
#endif
}

void printCurrentMode(int mode){
    // End of reel: Print crnt mode & Revs
    if (mode == 1)
    {
        lcd.setCursor(0, 0);
        lcd.print("End of reel");
        lcd.print("   ");
    }
    else if (mode == 2)
    {
        lcd.setCursor(0, 0);
        lcd.print("Relay Test");
        lcd.print("   ");
    }
    else if (mode == 3)
    {
        lcd.setCursor(0, 0);
        lcd.print("Hovering");
        lcd.print("   ");
    }
    else if (mode == 7)
    {
        lcd.setCursor(0, 0);
        lcd.print("Reel out");
        lcd.print("   ");

    }
    else if (mode == 8)
    {
        lcd.setCursor(0, 0);
        lcd.print("Reel in");
        lcd.print("   ");
    }
    else if (mode == 9)
    {
        lcd.setCursor(0, 0);
        lcd.print("Back to start");
        lcd.print("   ");
    }
    else if (mode == 0)
    {
        lcd.setCursor(0, 0);
        lcd.print("Stop");
        lcd.print("        ");
        lcd.setCursor(0, 1);
        lcd.print("        ");
    }
    else
    {
        // start-up
        lcd.setCursor(0, 0);
        lcd.print("Normal");
        lcd.setCursor(0, 1);
        lcd.print("1:E 2:R 3:H");
        lcd.print("   ");
    }
}

void printStatus(Data_t data)
{   
    // End of reel: Print crnt mode & Revs
    if (mode == 1)
    {
        lcd.setCursor(0, 1);
        lcd.print("Rev:   ");
        lcd.setCursor(5, 1);
        lcd.print(data.motor_revolutions);
        lcd.print(" ");
    }
    else if (mode == 2)
    {
        lcd.setCursor(0, 1);
        lcd.print("Rev:   ");
        lcd.setCursor(5, 1);
        lcd.print(data.motor_revolutions);
        lcd.print(" ");
    }
    else if (mode == 3)
    {
        lcd.setCursor(0, 1);
        lcd.print("Rev:   ");
        lcd.setCursor(5, 1);
        lcd.print(data.motor_revolutions);
        lcd.print(" ");

    }
    else if (mode == 7)
    {
        lcd.setCursor(0, 1);
        lcd.print("Rev: ");
        lcd.setCursor(5, 1);
        lcd.print(data.motor_revolutions);
        lcd.print(" ");
    }
    else if (mode == 8)
    {
        lcd.setCursor(0, 1);
        lcd.print("Rev: ");
        lcd.setCursor(5, 1);
        lcd.print(data.motor_revolutions);
        lcd.print(" ");
    }
    else if (mode == 9)
    {
        lcd.setCursor(0, 1);
        lcd.print("Rev: ");
        lcd.setCursor(5, 1);
        lcd.print(data.motor_revolutions);
        lcd.print(" ");
    }
    else if (mode == 0)
    {
        lcd.setCursor(0, 1);
        lcd.print("Rev: ");
        lcd.setCursor(5, 1);
        lcd.print(data.motor_revolutions);
        lcd.print(" ");
    }
    else
    {
        //startup
    }
}

// Task 1 
// void SendReceiveData(void *pvParameters)
// {
//     for (;;)
//     {
//         Data_t data;
//         if(xQueueReceive(xQueue, &data, 0) == pdPASS){
//             // Serial.print("Mode (C0): ");
//             // Serial.println(data.motor_mode);
//             // Serial.print("Revolutions (C0): ");
//             // Serial.println(data.motor_revolutions);
//             }
//         else{
//             // Serial.println("No msg received from Core 1");
//         }

//         if (coldStart)
//         {
//             master.connect(server, port);
//             Serial.println("Binding socket....");
//             coldStart = false;
//             vTaskDelay(100 / portTICK_PERIOD_MS);
//         }

//         if (master.connected())
//         {
//             connected = true;
//             // listen to vline msg/
//             // Serial.println("Connected!");
//             ReadVlineInfo();
//             // send stop rig signal

//             // Serial.println("Core 0 running! ");
//             // printCurrentMode();
//         }
//         else
//         {
//             master.connect(server, port);
//             Serial.println("Rebinding socket....");
//             vTaskDelay(2500/ portTICK_PERIOD_MS);
//         }
        
//         // Serial.print(xPortGetCoreID());
//         // Serial.print(" unused stack memory: ");
//         // Serial.println(uxTaskGetStackHighWaterMark(NULL));
        
//         vTaskDelay(5/ portTICK_PERIOD_MS);
//     }
// }

// Task 2
void RigControl(void *pvParameters){

    keypad.addEventListener(keypadEvent); // Add an event listener for this keypad
    // Runs motor control commands on Core 1
    for (;;){
        // Fetch key from keypad
        char key = keypad.getKey();
        // if (key) {
        //     Serial.println(key);
        // }

        motor();
        // Serial.print("Current mode: ");
        // Serial.println(mode);

        // Assign data to send_queue
        Data_t data;
        data.motor_mode = mode;
        data.motor_revolutions = revolutions;
        
        // Send integer to other task via queue
        if (xQueueSend(xQueue, &data, 5) != pdTRUE) {
            // Serial.println("ERROR: Could not put item on delay queue.");
        }

        // Limit the acceleration by updating at a limited rate
        vTaskDelay(10/ portTICK_PERIOD_MS);
    }
}

// Task 3
void RigDisplay(void *pvParameters){
    int display_mode = 0;
    int prev_mode = 0;
    for ( ;; )
    {
        // Serial.print(xPortGetCoreID());
        // Serial.print(" unused stack memory: ");
        // Serial.println(uxTaskGetStackHighWaterMark(NULL));
        Data_t data;
        if(xQueueReceive(xQueue, &data, 0) == pdPASS){
            // Fetching msgs from Task 2
        }else{
            // Serial.println("No msg received from Core 1");
        }

        // Update mode
        display_mode = data.motor_mode;
        if(display_mode != prev_mode){
            Serial.println("Update display once! ");
            lcd.clear(); // clear display once before mode change
            printCurrentMode(data.motor_mode);
            prev_mode = display_mode;
        }

        // Update progress and revolutions
        printStatus(data);

        vTaskDelay(20/ portTICK_PERIOD_MS);
    }
}

void setup()
{
    Serial2.begin(576000, SERIAL_8N1, RXD2, TXD2);
    Serial.begin(baud_rate);
    ser.begin(576000);
    lcd.begin();

    ser.set(buz.hz_,(uint16_t)1000);
    ser.set(buz.volume_,(uint8_t)250);
    ser.set(buz.duration_,(uint16_t)500);
    // Wire.begin();

    // WiFi.mode(WIFI_STA);
    // WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
    // WiFi.setHostname("ESP_RIG");
    // WiFi.begin(WIFI_SSID, WIFI_PASS);

    // while (WiFi.status() != WL_CONNECTED)
    // {
    //     vTaskDelay(500 / portTICK_PERIOD_MS);
    //     Serial.print(".");
    // }
    
    // WiFi.onEvent(WiFiStationDisconnected, SYSTEM_EVENT_STA_DISCONNECTED);

    xQueue = xQueueCreate(5, sizeof(Data_t));

    // xTaskCreatePinnedToCore(
    //     SendReceiveData, /* Task function. */
    //     "Send_Receive",  /* name of task. */
    //     2048,           /* Stack size of task */
    //     NULL,            /* parameter of the task */
    //     8,               /* priority of the task */
    //     &Task1,          /* Task handle to keep track of created task */
    //     0);              /* pin task to core 1 */
    
    xTaskCreatePinnedToCore(
        RigControl,     /* Task function. */
        "Controlling_Rig",  /* name of task. */
        5500,           /* Stack size of task */
        NULL,            /* parameter of the task */
        8,               /* priority of the task */
        &Task2,          /* Task handle to keep track of created task */
        1);              /* pin task to core 0 */
    
    xTaskCreatePinnedToCore(
        RigDisplay,     /* Task function. */
        "Update_display",  /* name of task. */
        2000,           /* Stack size of task */
        NULL,            /* parameter of the task */
        5,               /* priority of the task */
        &Task3,          /* Task handle to keep track of created task */
        1);              /* pin task to core 1*/
    
    coldStart = true;
    
    Serial.println("Setup complete!");
}

void loop(){}