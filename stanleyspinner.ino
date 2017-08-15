#include <Encoder.h>
#include <Firmata.h>
#include <LedControl.h>

#define DEBUG_MODE true
#define MAX_CS_PIN 5
#define NUM_DISPLAYS 3

// Change these two numbers to the pins connected to your encoder.
//   Best Performance: both pins have interrupt capability
//   Good Performance: only the first pin has interrupt capability
//   Low Performance:  neither pin has interrupt capability
Encoder myEnc(2, 3);
//   avoid using pins with LEDs attachedP

// LedControl
//                        MOSI              MISO              SCK             SS(Slave)   SS(Master)
// Mega1280 or Mega2560	  51 or ICSP-4	    50 or ICSP-1	  52 or ICSP-3	  53	        -
// Uno              	  11 or ICSP-4	    12 or ICSP-1	  13 or ICSP-3	  53	        -

// MAX7219 digit indices are (left-to-right) 87654321
// LedControl readjusts the idices to be 0-indexed (76543210)

LedControl lc = LedControl(MAX_CS_PIN, NUM_DISPLAYS);
long oldPosition = -999;
long oldVelocity = -999;
long oldAccel = -999;
float pollingRate = 1 / 15.0;
float pollTimer = 0.0;
unsigned long time;
unsigned long lastTime;
unsigned long deltaTime;

#define BAUDRATE 57600

void setup()
{
// digitalWrite(MAX_CS_PIN, HIGH);
#ifdef DEBUG_MODE
    Serial.begin(BAUDRATE);
#else
    Firmata.setFirmwareVersion(FIRMATA_FIRMWARE_MAJOR_VERSION, FIRMATA_FIRMWARE_MINOR_VERSION);
    Firmata.attach(STRING_DATA, stringCallback);
    Firmata.attach(START_SYSEX, sysexCallback);
    Firmata.begin(BAUDRATE);
#endif

    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(4, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    for (int i = 0; i < NUM_DISPLAYS; i++)
    {
        lc.shutdown(i, false);
        lc.clearDisplay(i);
        lc.setIntensity(i, 8);
    }


    // char test[] = {'1', '2', '3', '4', '5', '6', '7', '8'};
    // char test[] = {' ', ' ', '-', '4', 'a', '6', '7', '8'};
    // displayChar(0, test);
}

void stringCallback(char *myString)
{
    Firmata.sendString(myString);
    digitalWrite(LED_BUILTIN, HIGH);
}

void sysexCallback(byte command, byte argc, byte *argv)
{
    Firmata.sendSysex(command, argc, argv);
}

void blink(int num)
{
    for (int i = 0; i < num; i++)
    {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(200);
        digitalWrite(LED_BUILTIN, LOW);
        delay(200);
    }
}

void loop()
{
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

    // reset builtin led (data light)
    digitalWrite(LED_BUILTIN, LOW);
    digitalWrite(4, LOW);

    long newPosition = myEnc.read();

    if (newPosition != oldPosition)
    {
        digitalWrite(4, HIGH);
        String stringValue = "";
        stringValue += newPosition;
#ifdef DEBUG_MODE
        Serial.println(newPosition);
#else
        int stringLength = stringValue.length();
        char charArray[stringLength];
        stringValue.toCharArray(charArray, stringLength);
        Firmata.sendString(charArray);
#endif

        long velocity = oldPosition - newPosition;
        long accel = oldVelocity - velocity;

        
        if (updateDisplay)
        {
            String velString = "";
            velString += velocity;
            String accelString = "";
            accelString += accel;
            displayChar(0, stringValue);
            displayChar(1, velString);
            displayChar(2, accelString);
        }
        
        // do this last
        oldVelocity = velocity;
        oldPosition = newPosition;    
        oldAccel = accel;    
    }

#ifdef DEBUG_MODE
#else
    while (Firmata.available())
    {
        Firmata.processInput();
    }
#endif

    // delay(10);
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
