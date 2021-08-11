/*******************************************************************************
 * This sketch turns a PSX Guncon controller into a USB absolute mouse
 * or Joystick, using an Arduino Leonardo.
 *
 * It uses the PsxNewLib, ArduinoJoystickLibrary
 * and an edited version of AbsMouse Library.
 *
 * For details on PsxNewLib, see
 * https://github.com/SukkoPera/PsxNewLib
 *
 * For details on ArduinoJoystickLibrary, see
 * https://github.com/MHeironimus/ArduinoJoystickLibrary
 *
 * For details on AbsMouse, see
 * https://github.com/jonathanedgecombe/absmouse
 *
 * The guncon needs to "scan" the entire screen before it can properly send
 * the coorinates. Just point it at the screen and move slowly from side to side
 * and top to botom. The values will be stored as min and max, and will be used
 * to calculate the absolute mouse position.
 * It's recommended to use a full screen white image when doing this.
 *
 * When connected it will be in a not enabled state.
 * It can emulate a Mouse or a Joystick:
 *
 * Press Gun Trigger / Circle to enable mouse emulation.
 * Press Gun A (Left side) / Start to enable joystick emulation.
 *
 * To disable, point it off screen and press A + B + Trigger
 *
 * Buttons are mapped as follows:
 * A (Left side) / Start -> Mouse Right / Joystick btn 1
 * B (Right side) / Cross -> Mouse Middle / Joystick btn 2
 * Trigger / Circle -> Mouse Left / Joystick btn 0
*/


#include <PsxControllerHwSpi.h>
#include "AbsMouse.h"
#include <Joystick.h>

const byte PIN_PS2_ATT = 10;

const unsigned long POLLING_INTERVAL = 1000U / 400U;//needs more testing

// Send debug messages to serial port
//#define ENABLE_SERIAL_DEBUG

PsxControllerHwSpi<PIN_PS2_ATT> psx;

Joystick_ usbStick(
    JOYSTICK_DEFAULT_REPORT_ID,
    JOYSTICK_TYPE_JOYSTICK,
    3,      // buttonCount
    0,      // hatSwitchCount (0-2)
    true,   // includeXAxis
    true,   // includeYAxis
    false,    // includeZAxis
    false,    // includeRxAxis
    false,    // includeRyAxis
    false,    // includeRzAxis
    false,    // includeRudder
    false,    // includeThrottle
    false,    // includeAccelerator
    false,    // includeBrake
    false   // includeSteering
);


#ifdef ENABLE_SERIAL_DEBUG
#define dstart(spd) do {Serial.begin (spd); while (!Serial) {digitalWrite (LED_BUILTIN, (millis () / 500) % 2);}} while (0);

#define debug(...) Serial.print (__VA_ARGS__)
#define debugln(...) Serial.println (__VA_ARGS__)
#else
#define dstart(...)
#define debug(...)
#define debugln(...)
#endif

const byte ANALOG_DEAD_ZONE = 25U;

const word maxMouseValue = 32767;

//min and max possible values
//from document at http://problemkaputt.de/psx-spx.htm#controllerslightgunsnamcoguncon
//x is 77 to 461
//y is 25 to 248 (ntsc). y is 32 to 295 (pal)
//from personal testing on a pvm
//x is 72 to 450. with underscan x 71 to 453
//y is 22 to 248. with underscan y is 13 to 254 (ntsc)

const unsigned short int minPossibleX = 77;
const unsigned short int maxPossibleX = 461;
const unsigned short int minPossibleY = 25;
const unsigned short int maxPossibleY = 295;

const byte maxNoLightCount = 10;

boolean haveController = false;

// Minimum and maximum detected values. Varies from tv to tv.
// Values will be detected when pointing at the screen.
word minX = 1000;
word maxX = 0;
word minY = 1000;
word maxY = 0;

int offsetX = 0;
int offsetY = 0;

unsigned char noLightCount = 0;

// Last successful read coordinates
word lastX = -1;
word lastY = -1;

boolean enableReport = false;
boolean enableMouseMove = false;
boolean enableJoystick = false;

word convertRange(double gcMin, double gcMax, double value) {
    double scale = maxMouseValue / (gcMax - gcMin);
    return (word)((value - gcMin) * scale);
}

/*word convertRange(word gcMin, word gcMax, word value) {
  word scale = (word)(maxMouseValue / (gcMax - gcMin));
  return (word)((value - gcMin) * scale);
}*/

void moveToCoords(word x, word y) {
    if (enableMouseMove)
        AbsMouse.move(x, y);

    if (enableJoystick) {
        usbStick.setXAxis(x);
        usbStick.setYAxis(y);
    }
}

void releaseAllButtons() {
    if (enableMouseMove) {
        AbsMouse.release(MOUSE_LEFT);
        AbsMouse.release(MOUSE_RIGHT);
        AbsMouse.release(MOUSE_MIDDLE);
        AbsMouse.report();
    }

    if (enableJoystick) {
        usbStick.releaseButton(0);
        usbStick.releaseButton(1);
        usbStick.releaseButton(2);
        usbStick.sendState();
    }
}

void readGuncon() {
    word x, y, convertedX, convertedY;
    GunconStatus gcStatus;
    gcStatus = psx.getGunconCoordinates(x, y); //use coords from guncon

  /*
    if (gcStatus == GUNCON_OK)
      debugln (F("STATUS: GUNCON_OK!"));
    else if (gcStatus == GUNCON_UNEXPECTED_LIGHT)
      debugln (F("STATUS: GUNCON_UNEXPECTED_LIGHT!"));
    else if (gcStatus == GUNCON_NO_LIGHT)
      debugln (F("STATUS: GUNCON_NO_LIGHT!"));
    else
      debugln (F("STATUS: GUNCON_OTHER_ERROR!"));
  */

    if (gcStatus == GUNCON_OK) {
        noLightCount = 0;
        // is inside possible range?
        if (x >= minPossibleX && x <= maxPossibleX && y >= minPossibleY && y <= maxPossibleY) {
            //x += offsetX;
            //y += offsetY;
            lastX = x;
            lastY = y;

            //got new min or max values?
            if (x < minX)
                minX = x;
            else if (x > maxX)
                maxX = x;

            if (y < minY)
                minY = y;
            else if (y > maxY)
                maxY = y;

            if (enableMouseMove || enableJoystick) {
                convertedX = convertRange(minX, maxX, x);
                convertedY = convertRange(minY, maxY, y);
                moveToCoords(convertedX, convertedY);
            }
        }
    }
    else if (gcStatus == GUNCON_NO_LIGHT) {

        //up to 10 no_light reads will report the last good values
        if (lastX != 0 && lastY != 0) {
            convertedX = convertRange(minX, maxX, lastX);
            convertedY = convertRange(minY, maxY, lastY);

            moveToCoords(convertedX, convertedY);

            noLightCount++;

            if (noLightCount > maxNoLightCount) {
                noLightCount = 0;
                lastX = 0;
                lastY = 0;

                //set it offscreen (bottom left). need to test
                //also release all buttons
                if (enableMouseMove)
                    AbsMouse.move(0, maxMouseValue);

                //put joystick to the center position
                if (enableJoystick) {
                    usbStick.setXAxis(16383);
                    usbStick.setYAxis(16383);
                }

                releaseAllButtons();
            }
        }
        else if (psx.buttonPressed(PSB_CIRCLE) && psx.buttonPressed(PSB_START) && psx.buttonPressed(PSB_CROSS)) {//only when using guncon. dualshock wont work
            enableReport = false;
            releaseAllButtons();
            delay(1000);
        }

    }
}

void analogDeadZone(byte& value) {
    int8_t delta = value - ANALOG_IDLE_VALUE;
    if (abs(delta) < ANALOG_DEAD_ZONE)
        value = ANALOG_IDLE_VALUE;
}

void readDualShock() {
    word x, y;
    byte analogX = ANALOG_IDLE_VALUE;
    byte analogY = ANALOG_IDLE_VALUE;
    if (psx.getLeftAnalog(analogX, analogY)) { //use coords from analog controller
        analogDeadZone(analogX);
        analogDeadZone(analogY);
    }
    x = convertRange(ANALOG_MIN_VALUE, ANALOG_MAX_VALUE, analogX);
    y = convertRange(ANALOG_MIN_VALUE, ANALOG_MAX_VALUE, analogY);
    moveToCoords(x, y);

    if (psx.buttonPressed(PSB_SELECT)) {
        enableReport = false;
        releaseAllButtons();
        delay(1000);
    }
}

void handleButtons() {
    if (psx.buttonJustPressed(PSB_CIRCLE)) { //trigger press
        AbsMouse.press(MOUSE_LEFT);
        usbStick.pressButton(0);
    }
    else if (psx.buttonJustReleased(PSB_CIRCLE)) { //trigger release
        AbsMouse.release(MOUSE_LEFT);
        usbStick.releaseButton(0);
    }

    if (psx.buttonJustPressed(PSB_START)) { //A button press
        AbsMouse.press(MOUSE_RIGHT);
        usbStick.pressButton(1);
    }
    else if (psx.buttonJustReleased(PSB_START)) { //A button release
        AbsMouse.release(MOUSE_RIGHT);
        usbStick.releaseButton(1);
    }

    if (psx.buttonJustPressed(PSB_CROSS)) { //B button press
        AbsMouse.press(MOUSE_MIDDLE);
        usbStick.pressButton(2);
    }
    else if (psx.buttonJustReleased(PSB_CROSS)) { //B button release
        AbsMouse.release(MOUSE_MIDDLE);
        usbStick.releaseButton(2);
    }
}


void setup() {
#ifdef ENABLE_SERIAL_DEBUG
    pinMode(LED_BUILTIN, OUTPUT);
#endif

    usbStick.begin(false);

    usbStick.setXAxisRange(0, maxMouseValue);
    usbStick.setYAxisRange(0, maxMouseValue);

    dstart(115200);
    debugln(F("Ready!"));
}

void loop() {
    static unsigned long last = 0;

    if (millis() - last >= POLLING_INTERVAL) {
        last = millis();

        if (!haveController) {
            if (psx.begin()) {
                debugln(F("Controller found!"));

                haveController = true;
            }
        }
        else {
            noInterrupts();
            boolean isReadSuccess = psx.read();
            interrupts();

            if (!isReadSuccess) {
                //debug (F("Controller lost."));
                //debug (F(" last values: x = "));
                //debug (lastX);
                //debug (F(", y = "));
                //debugln (lastY);

                haveController = false;
            }
            else {
                // Read was successful, so let's make up data for Mouse

                if (!enableReport) {
                    if (!enableMouseMove && !enableJoystick) {
                        if (psx.buttonJustPressed(PSB_CIRCLE)) {
                            enableReport = true;
                            enableMouseMove = true;
                            //delay(300);
                            return;
                        }
                        else if (psx.buttonJustPressed(PSB_START)) {
                            enableReport = true;
                            enableJoystick = true;
                            //delay(300);
                            return;
                        }
                    }
                    else if (psx.buttonJustPressed(PSB_CIRCLE) || psx.buttonJustPressed(PSB_START) || psx.buttonJustPressed(PSB_CROSS)) {
                        enableReport = true;
                        //delay(300);
                        return;
                    }
                }

                if (enableReport) {
                    handleButtons();
                    PsxControllerProtocol proto = psx.getProtocol();
                    switch (proto) {
                    case PSPROTO_GUNCON:
                        readGuncon();
                        break;
                    case PSPROTO_DUALSHOCK:
                    case PSPROTO_DUALSHOCK2:
                        readDualShock();
                        break;
                    default:
                        return;
                    }
                }

                //todo else
                if (enableReport) {
                    if (enableMouseMove)
                        AbsMouse.report();
                    else if (enableJoystick)
                        usbStick.sendState();
                }

            }
        }
    }
}
