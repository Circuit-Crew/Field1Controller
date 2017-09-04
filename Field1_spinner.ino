#include <Boards.h>
#include <Firmata.h>
#include <FirmataConstants.h>
#include <FirmataDefines.h>
#include <FirmataMarshaller.h>
#include <FirmataParser.h>

#include <Encoder.h>
#include <Firmata.h>
#include <LedControl.h>

#define DEBUG_MODE false
#define MAX_CS_PIN 8
#define NUM_DISPLAYS 3
#define DEBUG_LED_PIN 12

#define PLAYER_1 4
#define PLAYER_2 5
#define PLAYER_3 6
#define PLAYER_4 7

// Change these two numbers to the pins connected to your encoder.
//   Best Performance: both pins have interrupt capability
//   Good Performance: only the first pin has interrupt capability
//   Low Performance:  neither pin has interrupt capability
Encoder myEnc(2, 3);
Encoder myEnc2(A4, A5);
//   avoid using pins with LEDs attachedP

// LedControl
//                        MOSI              MISO              SCK             SS(Slave)   SS(Master)
// Mega1280 or Mega2560	  51 or ICSP-4	    50 or ICSP-1	  52 or ICSP-3	  53	        -
// Uno              	  11 or ICSP-4	    12 or ICSP-1	  13 or ICSP-3	  53	        -

// MAX7219 digit indices are (left-to-right) 87654321
// LedControl readjusts the idices to be 0-indexed (76543210)

LedControl lc = LedControl(MAX_CS_PIN, NUM_DISPLAYS);
long oldPosition = -999;
long oldPosition2 = -999;
long oldVelocity = -999;
long oldAccel = -999;
float pollingRate = 1 / 35.0;
float pollTimer = 0.0;
unsigned long time;
unsigned long lastTime;
unsigned long deltaTime;
String display[2];

int buttonPinArray[4] = {PLAYER_1, PLAYER_2, PLAYER_3, PLAYER_4};
int previousButtonState[4] = {false, false, false, false};
int currentButtonState[4] = {false, false, false, false};
float debounceTime = 100.0f;

#define BAUDRATE 57600

byte previousPIN[TOTAL_PORTS]; // PIN means PORT for input
byte previousPORT[TOTAL_PORTS];

void outputPort(byte portNumber, byte portValue)
{
    // only send the data when it changes, otherwise you get too many messages!
    if (previousPIN[portNumber] != portValue)
    {
        Firmata.sendDigitalPort(portNumber, portValue);
        previousPIN[portNumber] = portValue;
    }
}

void digitalWriteCallback(byte port, int value)
{
    byte i;
    byte currentPinValue, previousPinValue;

    if (port < TOTAL_PORTS && value != previousPORT[port])
    {
        for (i = 0; i < 8; i++)
        {
            currentPinValue = (byte)value & (1 << i);
            previousPinValue = previousPORT[port] & (1 << i);
            if (currentPinValue != previousPinValue)
            {
                digitalWrite(i + (port * 8), currentPinValue);
            }
        }
        previousPORT[port] = value;
    }
}

void stringCallback(char *myString)
{
    Firmata.sendString(myString);
}

void sysexCallback(byte command, byte argc, byte *argv)
{
    Firmata.sendSysex(command, argc, argv);
}

void setPinModeCallback(byte pin, int mode)
{
    if (IS_PIN_DIGITAL(pin))
    {
        pinMode(PIN_TO_DIGITAL(pin), mode);
    }
}

void setup()
{
    // digitalWrite(MAX_CS_PIN, HIGH);

    pinMode(DEBUG_LED_PIN, OUTPUT);
    digitalWrite(DEBUG_LED_PIN, LOW);

    // Set button pins to input and init them as LOW
    for (int i = 0; i < 4; i++)
    {
        digitalWrite(buttonPinArray[i], LOW);
        pinMode(buttonPinArray[i], INPUT);
    }

    if (DEBUG_MODE)
    {
        Serial.begin(BAUDRATE);
    }
    else
    {
        String name = "Spinner";
        Firmata.setFirmwareNameAndVersion(name.c_str(), FIRMATA_FIRMWARE_MAJOR_VERSION, FIRMATA_FIRMWARE_MINOR_VERSION);
        Firmata.attach(STRING_DATA, stringCallback);
        Firmata.attach(START_SYSEX, sysexCallback);
        Firmata.attach(SET_PIN_MODE, setPinModeCallback);
        Firmata.attach(DIGITAL_MESSAGE, digitalWriteCallback);
        Firmata.begin(BAUDRATE);
    }

    for (int i = 0; i < NUM_DISPLAYS; i++)
    {
        lc.shutdown(i, false);
        lc.clearDisplay(i);
        lc.setIntensity(i, 1);
    }

    // char test[] = {'1', '2', '3', '4', '5', '6', '7', '8'};
    // char test[] = {' ', ' ', '-', '4', 'a', '6', '7', '8'};
    // displayChar(1, test);
}

void loop()
{

    byte i;
    for (i = 0; i < TOTAL_PORTS; i++)
    {
        outputPort(i, readPort(i, 0xff));
    }

    if (!DEBUG_MODE)
    {
        while (Firmata.available())
        {
            Firmata.processInput();
        }
    }

    // time
    lastTime = time;
    time = millis();
    deltaTime = time - lastTime;

    pollTimer += deltaTime;

    bool updateDisplay = false;
    if (pollTimer > pollingRate)
    {
        updateDisplay = true;
        pollTimer = 0;
    }

    checkButtons();

    long newPosition = myEnc.read();
    String s1 = checkEncoder(newPosition, oldPosition);
    display[0] = s1;
    long newPosition2 = myEnc2.read();
    String s2 = checkEncoder(newPosition2, oldPosition2);
    display[1] = s2;

    long velocity = oldPosition - newPosition;
    long accel = oldVelocity - velocity;

    if (updateDisplay)
    {
        for (int i = 0; i < 2; i++)
        {
            displayChar(i, display[i]);
        }
        // displayChar(1, s2);
        // displayChar(0, s2);
    }

    // if (updateDisplay)
    // {
    //     String velString = "";
    //     velString += velocity;
    //     String accelString = "";
    //     accelString += accel;
    //     displayChar(0, stringValue);
    //     displayChar(1, velString);
    //     displayChar(2, accelString);
    // }

    // do this last
    oldVelocity = velocity;
    oldPosition = newPosition;
    oldPosition2 = newPosition2;
    oldAccel = accel;
}

String checkEncoder(long newPos, long oldPos)
{
    String stringValue = "";
    stringValue += newPos;
    if (newPos != oldPos)
    {
        if (DEBUG_MODE)
        {
            Serial.println(newPos);
        }
        else
        {
            int stringLength = stringValue.length();
            char charArray[stringLength];
            stringValue.toCharArray(charArray, stringLength);
            Firmata.sendString(charArray);
        }
    }
    return stringValue;
}

// When debugging the arduino will handle the state checks itself
// When connected to Field-1 it will just send the raw pin state and Field-1 will handle it
void checkButtons()
{
    bool anyButtonPressed = false;
    // 1 2 3 4
    for (int i = 0; i < 4; i++)
    {
        currentButtonState[i] = digitalRead(buttonPinArray[i]);
        if (currentButtonState[i] == HIGH)
            anyButtonPressed = true;
        if (DEBUG_MODE)
        {
            if (currentButtonState[i] == previousButtonState[i])
            {
            }
            else if (currentButtonState[i] == HIGH)
            {
                String debugString = "Button ";
                debugString += i;
                debugString += " pressed! [";
                debugString += time / 1000;
                debugString += "s]";
                Serial.println(debugString);
            }
            else if (currentButtonState[i] == LOW)
            {
                String debugString = "Button ";
                debugString += i;
                debugString += " released! [";
                debugString += time / 1000;
                debugString += "s]";
                Serial.println(debugString);
            }
        }
        else
        {
            if (currentButtonState[i] != previousButtonState[i])
            {
                if (currentButtonState[i] == HIGH)
                    anyButtonPressed = true;
                // make sure the first parameter is the pin number not the index in the for-loop

                // Firmata.sendDigitalPort((byte)buttonPinArray[i], (byte)currentButtonState[i]);
            }
        }
        previousButtonState[i] = currentButtonState[i];
    }
    digitalWrite(DEBUG_LED_PIN, anyButtonPressed);
    // Firmata.sendDigitalPort(DEBUG_LED_PIN, digitalRead(DEBUG_LED_PIN));
}

void displayNumber(int addr, byte data[])
{
    lc.clearDisplay(addr);
    for (int i = 0; i < 8; i++)
    {
        lc.setDigit(addr, i, data[i], false);
    }
}

void displayNumber(int addr, long value)
{
    byte testDisplay[8];
    convertToDigits(testDisplay, value);
    displayNumber(addr, testDisplay);
}

void displayChar(int addr, String data)
{
    lc.clearDisplay(addr);
    int maxLength = 8;
    int length = data.length();

    if (length > maxLength)
    {
        // do something
        return;
    }

    char charArray[length];
    data.toCharArray(charArray, length);
    // strncpy(charArray, data.c_str(), stringLength);

    int firstIndex = 7 - length;
    for (int digit = 0; digit < length; digit++)
    {
        lc.setChar(addr, digit, data[length - 1 - digit], false);
    }
}

// Simple method for displaying char[8] arrays
void displayChar(int addr, char data[])
{
    for (int digit = 0; digit < 8; digit++)
    {
        lc.setChar(addr, 7 - digit, data[digit], false);
    }
}

void convertToDigits(byte output[], unsigned long value)
{
    unsigned long digitsCounter = value;

    for (int i = 0; i < 8; i++)
    {
        output[i] = digitsCounter % 10;

        // if (digitsCounter < pow(10, i)) // 1, 10, 100, 1000....
        //     output[i] = 0x0F;          // set the data to something out of range so the chip doesn't display anything on that digit -- set to 0x00 to pad with 0s (or any other valid value)

        digitsCounter /= 10;
    }
}
