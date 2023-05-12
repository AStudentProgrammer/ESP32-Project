#include <Arduino.h>
#include <iq_module_communication.hpp>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

WiFiClient master;
IPAddress server(192, 168, 0, 1);
const int baud_rate = 115200;
const int port = 8266;

// Make an IqSerial object using Serial2 (same as Serial)
IqSerial ser(Serial2);
// Make a PropellerMotorControlClient to interface with a motor module (ID 0)
BrushlessDriveClient mot(0);
PropellerMotorControlClient prop(0);
TemperatureMonitorUcClient tmp(0);
PowerMonitorClient pwr(0);
BuzzerControlClient buz(0);

/*##################################################### 
#                  Debugging Serial                   #
######################################################*/
// #define angleAndRevolutionSerial
// #define speedSerial
// #define setSpeedSerial
// #define currentSerial
// #define tempSerial

/*##################################################### 
#              Motor Control Variables                #
######################################################*/
// Mode of testing
int mode = 99;
int modeTime = 0;
int targetSpeed = 0;
int motorStatus;
int slowSpeedCount = 0;
int revolutions = 0;
int prevRev = 0;
int targetRev;
int revtimeDiff;
int endOfReelTimer = 0;
const int ENDOFREELHOVERTIME = 300;
int hoverTime = 0;
const int HOVERTIMECHANGE = 4;

float velocity = 0.0f;
float prevAngle;
float angle;
float temperature = 0.0f;
// Current measurements
float inCurr;
float outcurr;

boolean actualSpeedReached = false;
boolean positiveVel = true;
boolean up = false;
boolean stopWheel = false;
boolean fromStart = false; // one off actualSpeedReached when reel reaches 20m from 0m
boolean hover = false;
boolean resetPressed = false;
boolean beeped = false;
int timeStamp, timeDiff;
int dir = 0;

/*##################################################### 
#                   Keypad  Array                     #
######################################################*/
const byte ROWS = 4; // four rows
const byte COLS = 4; // three columns
char keys[ROWS][COLS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}};

byte rowPins[ROWS] = {14, 27, 26, 25}; // connect to the row pinouts of the keypad
byte colPins[COLS] = {33, 32, 2, 0};   // connect to the column pinouts of the keypad

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Set the LCD address to 0x27 (or 0x3F) for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x27, 16, 2);
int progressCount = 0;
int dataint, currentMode = 0;

/*################################################################################## 
#  Vline Messages (Only required if rig is connected to Vline)                     #
################################################################################### */
// Set WiFi credentials
// #define WIFI_SSID "VLINE_1023"
// #define WIFI_PASS "hQ57sJio"
// int batteryWarningStatus = 0;
// int mCurrentStatus = 0;
// int tCurrentStatus = 0;
// int bVoltageInStatus = 0;
// int bVoltageOutStatus = 0;
// int droneOnOffStatus = 0;
// int tensionStatus = 0;
// int laserEncoderErrorStatus = 0;
// int relayOn = 0;
// int temp_batteryWarningStatus = 0;
// int temp_mCurrentStatus = 0;
// int temp_tCurrentStatus = 0;
// int temp_bVoltageInStatus = 0;
// int temp_bVoltageOutStatus = 0;
// int temp_droneOnOffStatus = 0;
// int temp_tensionStatus = 0;
// int temp_laserEncoderErrorStatus = 0;
// int temp_relayOn = 0;

// char VlineToRigBuf[256];
// bool connectSucess;
// bool connected = false;
bool coldStart = false;

int ReadSerial(int readch, char *buffer, int len)
{
    static int pos = 0;
    int rpos;
    static boolean startReading = false;

    if (readch > 0)
    {
        switch (readch)
        {
        case '\r': // Ignore CR
            break;
        case '\n': // Return on new-line
            rpos = pos;
            pos = 0; // Reset position index ready for next time
            startReading = false;
            return rpos;
        case 'G':
            startReading = true;
            if (pos < len - 1)
            {
                buffer[pos++] = readch;
                buffer[pos] = 0;
            }
            break;
        case 'E':
            startReading = true;
            if (pos < len - 1)
            {
                buffer[pos++] = readch;
                buffer[pos] = 0;
            }
            break;
        case 'S':
            startReading = true;
            if (pos < len - 1)
            {
                buffer[pos++] = readch;
                buffer[pos] = 0;
            }
            break;
        case 'V':
            startReading = true;
            if (pos < len - 1)
            {
                buffer[pos++] = readch;
                buffer[pos] = 0;
            }
            break;
        default:
            if (startReading)
            {
                if (pos < len - 1)
                {
                    buffer[pos++] = readch;
                    buffer[pos] = 0;
                }
            }
        }
    }
    return 0;
}

void ParseSerial(char *_buf)
{
    if (_buf[0] == '1')
    {
        modeTime = 0;
        mode = 1;
        Serial.println("Doing Relay/stress test");
    }
    else if (_buf[0] == '2')
    {
        modeTime = 0;
        mode = 2;
        Serial.println("Doing 10m hovering test");
    }
    else if (_buf[0] == '3')
    {
        modeTime = 0;
        mode = 3;
        Serial.println("Doing 20m hovering test");
    }
    else if (_buf[0] == '4')
    {
        modeTime = 0;
        mode = 4;
        Serial.println("Doing 30m hovering test");
    }
    else if (_buf[0] == '5')
    {
        modeTime = 0;
        mode = 5;
        Serial.println("Doing 40m hovering test");
    }
    else if (_buf[0] == '6')
    {
        modeTime = 0;
        mode = 6;
        Serial.println("Doing 50m hovering test");
    }
    else if (_buf[0] == '7')
    {
        modeTime = 0;
        mode = 7;
        Serial.println("Doing 60m hovering test");
    }
    else if (_buf[0] == '8')
    {
        modeTime = 0;
        mode = 8;
        Serial.println("Doing end of reel test");
    }
    else if (_buf[0] == '9')
    {
        modeTime = 0;
        mode = 9;
        Serial.println("Reeling back to 0m");
    }
    else if (_buf[0] == 't')
    {
        resetPressed = true;
        Serial.println("Revolutions reset to 0 if motor not moving");
    }
    else
    {
        modeTime = 0;
        mode = 0;
        Serial.println("Standy mode");
    }
}

//speed - hardcode (estimate)
//1m/s = 16
//2m/s = 34
//3m/s = 60

void motorControl(int status)
{
    motorStatus = status;
    if (status == 0)
    {
        targetSpeed = 16.0f;
    }
    else if (status == 1)
    {
        // going up 2m/s
        // targetSpeed = 60.0f;
        targetSpeed = 34.0f;
    }
    else if (status == 2)
    {
        // going down 2m/s
        targetSpeed = -34.0f;
    }
    else if (status == 3)
    {
        targetSpeed = 34.0f;
    }
    else if (status == 4)
    {
        targetSpeed = 5.0f;
    }
    else if (status == 5)
    {
        targetSpeed = -16.0f;
    }
    else if (status == 6)
    {
        targetSpeed = 0.0f;
    }
    else if (status == 7)
    {
        targetSpeed = -4.0f;
    }
    else if (status == 8)
    {
        targetSpeed = 4.0f;
    }
    else if (status == 9)
    {
        targetSpeed = -34.0f;
    }
}

void motorControl_2(float _mpsSpeed){
    // int _state = _mpsSpeed*2+0.5; //round to nearest 0.5
    _mpsSpeed = _mpsSpeed > 7 ? 7 : (_mpsSpeed < -7 ? -7 : _mpsSpeed); //constrain to +- 7m/s

    //eqn can be given by speed = mps*16 +- 10
    targetSpeed = _mpsSpeed * 16.0;
    //targetSpeed = targetSpeed > 0 ? targetSpeed + 10 : targetSpeed - 10;
}

int desiredSpeedZero()
{
    return targetSpeed = 0;
}

int desiredSpeed()
{
    return targetSpeed;
}

boolean slowSpeed()
{
    if (slowSpeedCount <= 5)
    {
        return false;
    }
    else
    {
        return true;
    }
}

void relayEnduranceTest()
{
    // Reel the cable 20m first
    //  if (revolutions == 0) {
    //    motorControl(0);
    //    fromStart = true;
    //  }
    //  Serial.println("Doing mode 1!");
    if (fromStart)
    {
        if (revolutions < 40)
            motorControl(0); // targetSpeed =   16.0f
        else if (revolutions < 50)
            // motorControl(4); // targetSpeed =   16.0f
            motorControl_2(0.4);
        else if (revolutions > 60)
            motorControl(5); // targetSpeed =  -16.0f
        else if (revolutions > 50)
            // motorControl(7); // targetSpeed =  -16.0f
            motorControl_2(0.375);
        else
        {
            motorControl(0);
            actualSpeedReached = false;
            fromStart = false;
        }
    }
    // Reel 3m/s 30m-50m
    else if (prevRev == 64 && revolutions == 63)
    {
        motorControl(0);
        actualSpeedReached = false;
    }
    // Reel 2m/s 50m-20m
    else if (prevRev == 153 && revolutions == 154)
    {
        motorControl(5);
        actualSpeedReached = false;
    }
    /*
    Check if within cable limit
    If outside limit, code will stop motor. Need to restart
    */
    else if (revolutions >= 165)
    {
        ser.set(prop.ctrl_velocity_, 0.0f);
        stopWheel = true;
        return;
    }
    prevRev = revolutions;
}

// Joshua said that it is better to hover at 1m per minute to avoid stressing the cables
void newHoveringTest(int startHeight)
{
    // input -> 131
    // Serial.println("Doing mode 2!");
    int timeDiff = millis() - timeStamp;
    if (fromStart)
    {
        targetRev = startHeight;
        timeStamp = millis();
        fromStart = false;
    }
    else if (timeDiff > 12000)
    {
        if (dir == 0)
        {
            targetRev += 5;
        }
        else if (dir == 1)
        {
            targetRev -= 5;
        }
        timeStamp = millis();
        beeped = false; // reset beep
    }
    if (prevRev < 168 && revolutions > 168)
        dir = 1;
    else if (prevRev > 10 && revolutions < 10)
        dir = 0;
    revtimeDiff = targetRev - revolutions;

    if (revtimeDiff > 5)
    {
        hover = false;
        motorControl(3); // targetSpeed =  34.0f
    }
    else if (revtimeDiff <= 5 && revtimeDiff > 0)
    {
        hover = false;
        motorControl(0); // targetSpeed =  16.0f
    }
    else if (revtimeDiff < 0 && revtimeDiff >= -5)
    {
        hover = false;
        motorControl(5); // targetSpeed = -16.0f
    }
    else if (revtimeDiff < -5)
    {
        hover = false;
        motorControl(2); // targetSpeed = -34.0f
    }
    else
    {
        // target reached
        hover = true;
        // play tone once at here
        if (hover == true && !beeped)
        {
            ser.set(buz.hz_, (uint16_t)1000);
            ser.set(buz.volume_, (uint8_t)500);
            ser.set(buz.duration_, (uint16_t)1000);
            ser.set(buz.ctrl_note_);
            beeped = true;
        }

        hoverTime++;
        if (up)
            targetSpeed = 5.0f;
        else
            targetSpeed = -5.0f;

        if (hoverTime >= HOVERTIMECHANGE)
        {
            hoverTime = 0;
            up = !up;
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

// void manualControl(){
//   // mode m
//   // W for reel out, S for reel in, A for speed +, D for speed -
//   Serial.parse("w");
//   for Case W:
//       targetSpeed = 2.0f;
//   for Case S:
//       targetSpeed = 2.0f;
//   for Case A:
//       targetSpeed += 2.0f;
//   for Case D:
//       targetSpeed -= 2.0f;

// }

void endOfReelTest()
{
    // Serial.println("Doing end of reel!");
    // Reel to 50m at 2m/s
    // if (revolutions <= 140) {
    //   motorControl(3);
    // }
    // Reel to 60m at 1m/s
    if (revolutions <= 143)
    {
        motorControl(0);
    }
    // Slowly reel from 60-64m at 0.125m/s
    else if (revolutions == 165 || revolutions == 166)
    {
        motorControl(4);
        endOfReelTimer = 0;
    }
    // Reel back to 60m at 1m/s
    // else if (revolutions >= 194)
    // {
    //     if (endOfReelTimer < ENDOFREELHOVERTIME)
    //     {
    //         motorControl(4);
    //         endOfReelTimer++;
    //         return;
    //     }
    //     motorControl(7);
    // }

    else if ((revolutions >= 175))
    {
        static bool reset;

        if (revolutions >= 190)
        {
            reset = true;
        }

        if (!reset)
        {
            // motorControl(4);
            motorControl_2(0.5);
        }

        if (reset)
        {
            // motorControl(7);
            motorControl_2(-0.375);
            if (revolutions == 175 || revolutions == 174)
            {
                reset = false;
            }
        }

        return;
    }
}

float velocityInfo()
{
    if (ser.get(mot.obs_velocity_, velocity))
    {
        // converting from rad/s to m/s
        velocity *= 0.06;
        // To keep track if motor is slowed down for too long
        if (actualSpeedReached &&
            ((motorStatus == 1 && velocity < 3.0f) ||
             (motorStatus == 2 && velocity < -2.3f) ||
             (motorStatus == 3 && velocity < 1.7f)))
        {
            slowSpeedCount++;
        }
        else
        {
            slowSpeedCount = 0;
        }
        if (velocity > 0)
        {
            positiveVel = true;
        }
        else
        {
            positiveVel = false;
        }
#ifdef speedSerial
        Serial.print("Measured Velocity :");
        Serial.print(velocity);
        Serial.println(" m/s");
#endif
    }
    return velocity;
}

void stop()
{
    static float BRAKE_STRENGTH = 3.0;
    if (velocityInfo() > 0)
    {
        motorControl_2(-velocityInfo()-BRAKE_STRENGTH);
    }
    else
    {
        motorControl_2(-velocityInfo()+BRAKE_STRENGTH);
    }
}

void numberOfRevs()
{
    if (ser.get(mot.obs_angle_, angle))
    {
        if (positiveVel && prevAngle > 0 && angle <= 0)
        {
            revolutions++;
        }
        else if (!positiveVel && prevAngle < 0 && angle >= 0)
        {
            revolutions--;
        }
        if (resetPressed)
        {
            if (prevAngle == angle)
            {
                revolutions = 0;
            }
            resetPressed = false;
        }
        prevAngle = angle;
#ifdef angleAndRevolutionSerial
        Serial.print("Millis: ");
        Serial.print(millis());
        Serial.print(" Angle: ");
        Serial.print(angle);
        Serial.print(" Revolutions: ");
        Serial.println(revolutions);
#endif
    }
}

void keypadEvent(KeypadEvent key)
{
    switch (keypad.getState())
    {
    case PRESSED:
        // clear the screen every time the mode changes
        // lcd.clear();
        if (key == '1')
        {
            modeTime = 0;
            mode = 1;
            Serial.println("1 pressed!");
        }
        if (key == '2')
        {
            modeTime = 0;
            mode = 2;
            Serial.println("2 pressed!");
        }
        if (key == '3')
        {
            modeTime = 0;
            mode = 3;
            Serial.println("3 pressed!");
        }
        if (key == '4')
        {
            modeTime = 0;
            mode = 4;
            Serial.println("4 pressed!");
        }
        if (key == '5')
        {
            modeTime = 0;
            mode = 5;
            Serial.println("5 pressed!");
        }
        if (key == '6')
        {
            modeTime = 0;
            mode = 6;
            Serial.println("6 pressed!");
        }
        if (key == '7')
        {
            modeTime = 0;
            mode = 7;
            Serial.println("7 pressed!");
        }
        if (key == '8')
        {
            modeTime = 0;
            mode = 8;
            Serial.println("8 pressed!");
        }
        if (key == '9')
        {
            modeTime = 0;
            mode = 9;
            Serial.println("9 pressed!");
        }
        if (key == '0')
        {
            modeTime = 0;
            mode = 0;
            Serial.println("0 pressed!");
        }
        if (key == 'D')
        {
            modeTime = 0;
            mode = 99;
            Serial.println("D pressed!");
        }
        break;

        // case RELEASED:
        //     if (key == '*') {
        //         digitalWrite(ledPin,ledPin_state);    // Restore LED state from before it started blinking.
        //         blink = false;
        //     }
        //     break;

        // case HOLD:
        //     if (key == '*') {
        //         blink = true;    // Blink the LED when holding the * key.
        //     }
        //     break;
        // }
    }
}

void clearDisplay()
{
    lcd.setCursor(0, 0);
    lcd.print("           ");
    lcd.setCursor(0, 1);
    lcd.print("           ");
}

// void CheckConnection()
// {
//     if (WiFi.status() != WL_CONNECTED)
//     {
//         // Retry connection
//         Serial.println("Lost WI-FI connection");
//         WiFi.begin(WIFI_SSID, WIFI_PASS);
//     }
// }

// void ReadVlineInfo()
// {
//     if (ReadSerial(master.read(), VlineToRigBuf, sizeof(VlineToRigBuf)) > 0)
//     {
//         // Serial.println("Reading from Vline: ");
//         // Serial.println(VlineToRigBuf);
//         if (sscanf(VlineToRigBuf, "E,%d,%d,%d,%d,%d,%d,%d,%d,%d",
//                    &temp_batteryWarningStatus, &temp_mCurrentStatus, &temp_tCurrentStatus, &temp_bVoltageInStatus, &temp_bVoltageOutStatus, &temp_droneOnOffStatus, &temp_tensionStatus, &temp_laserEncoderErrorStatus, &temp_relayOn) == 9)
//         {
//             batteryWarningStatus = temp_batteryWarningStatus;
//             mCurrentStatus = temp_mCurrentStatus;
//             tCurrentStatus = temp_tCurrentStatus;
//             bVoltageInStatus = temp_bVoltageInStatus;
//             bVoltageOutStatus = temp_bVoltageOutStatus;
//             droneOnOffStatus = temp_droneOnOffStatus;
//             tensionStatus = temp_tensionStatus;
//             laserEncoderErrorStatus = temp_laserEncoderErrorStatus;
//             relayOn = temp_relayOn;
//         }

//         // Serial.print("Relay: ");
//         // Serial.println(relayOn);
//     }
// }

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
    WiFi.disconnect();
    Serial.println("Reconnecting....");
    WiFi.reconnect();
}

void serialPrint()
{
#ifdef setSpeedSerial
    float vel;
    if (ser.get(mot.obs_velocity_, vel))
    {
        Serial.print("Prop speed: ");
        Serial.println(vel);
    }
#endif
#ifdef currentSerial
    if (ser.get(pwr.amps_, inCurr))
    {
        Serial.print("Input curent :");
        Serial.print(inCurr);
        Serial.print("A   ");
    }
    if (ser.get(mot.est_motor_amps_, outcurr))
    {
        Serial.print("Output curent :");
        Serial.print(outcurr);
        Serial.print("A   ");
    }
#endif
#ifdef tempSerial
    if (ser.get(tmp.uc_temp_, temperature))
    {
        Serial.print("Motor temperature: ");
        Serial.println(temperature);
    }
#endif
}