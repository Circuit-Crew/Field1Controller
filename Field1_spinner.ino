#include <Boards.h>
#include <FirmataConstants.h>
#include <FirmataDefines.h>
#include <FirmataMarshaller.h>
#include <FirmataParser.h>

#include <Encoder.h>
#include <Firmata.h>
#include <LedControl.h>

#define DEBUG_MODE false
#define ENABLE_MAX_DISPLAY true // if using MAX7219 modules for spinner debugging
#define MAX_CS_PIN 8
#define NUM_DISPLAYS 3
#define DEBUG_LED_PIN 12
#define NUM_ENCODER 2

#define BUTTON_1 4
#define BUTTON_2 5
#define BUTTON_3 6
#define BUTTON_4 7
#define ENCODER_OUTPUT_1 A0
#define ENCODER_OUTPUT_2 A1
#define ENCODER_OUTPUT_3 A2
#define ENCODER_OUTPUT_4 A3
#define BAUDRATE 57600

// Change these two numbers to the pins connected to your encoder.
//   Best Performance: both pins have interrupt capability
//   Good Performance: only the first pin has interrupt capability
//   Low Performance:  neither pin has interrupt capability
Encoder* spinner[NUM_ENCODER];
Encoder myEnc =  Encoder(2, 3);
Encoder myEnc2 = Encoder(A4, A5);
// Encoder myEnc3(A4, A5);
// Encoder myEnc4(A4, A5);

//   avoid using pins with LEDs attachedP

// LedControl
//                          MOSI                    MISO                            SCK                         SS(Slave)            SS(Master)
// Mega         	   51 or ICSP-4	         50 or ICSP-1	            52 or ICSP-3	        53	                     -
// Uno              	 11 or ICSP-4	      12 or ICSP-1	             13 or ICSP-3	          53	                    -

// MAX7219 digit indices are (left-to-right) 87654321
// LedControl readjusts the idices to be 0-indexed (76543210)

LedControl lc = LedControl(MAX_CS_PIN, NUM_DISPLAYS);
long previousPos[NUM_ENCODER];
long previousVel[NUM_ENCODER];
long previousAccel[NUM_ENCODER];
String display[NUM_ENCODER];
float pollingRate = 1 / 35.0;
float pollTimer = 0.0;
unsigned long time;
unsigned long lastTime;
unsigned long deltaTime;

float debounceTime = 100.0f;

byte encoderOutputPins[4] = { ENCODER_OUTPUT_1, ENCODER_OUTPUT_2, ENCODER_OUTPUT_3, ENCODER_OUTPUT_4};

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

    // Setup rotary encoders
    spinner[0] = &myEnc;
    spinner[1] = &myEnc2;

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
    if (DEBUG_MODE)
    {
        debugUpdate();
        return;
    }

    byte i;
    for (i = 0; i < TOTAL_PORTS; i++)
    {
        outputPort(i, readPort(i, 0xff));
    }

    while (Firmata.available())
    {
        Firmata.processInput();
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

    updateEncoders(updateDisplay);

}

void debugUpdate()
{
}

void updateEncoders(bool updateDisplay)
{
    for (int i = 0; i < NUM_ENCODER; i++)
    {
        long newPosition = spinner[i]->read();
        display[i] = checkEncoder(newPosition, previousPos[i]);
        long velocity = previousPos[i] - newPosition;
        long accel = previousAccel[i] - velocity;
        previousPos[i] = newPosition;

        analogWrite(encoderOutputPins[i], (int) velocity);
    }

    if (updateDisplay)
    {
        for (int i = 0; i < NUM_DISPLAYS; i++)
        {
            displayChar(i, display[i]);
        }
    }
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
