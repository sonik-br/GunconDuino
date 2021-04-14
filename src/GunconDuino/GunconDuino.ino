/*******************************************************************************
 * This sketch turns a PSX Guncon controller into a USB absolute mouse,
 * using an Arduino Leonardo.
 * 
 * It uses the PsxNewLib and an edited version of AbsMouse Library.
 * 
 * For details on PsxNewLib, see
 * https://github.com/SukkoPera/PsxNewLib
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
 * To enable simply press the trigger.
 * To disable, point it off screen and press A + B + Trigger
 * 
 * Buttons are mapped as follows:
 * A (Left side) -> Start [PSB_START] -> Mouse Right
 * B (Right side) -> Cross pPSB_CROSS] -> Mouse Middle
 * Trigger -> Circle [PSB_CIRCLE] -> Mouse Left
*/


#include <PsxControllerBitBang.h>
#include "AbsMouse.h"

/* We must use the bit-banging interface, as SPI pins are only available on the
 * ICSP header on the Leonardo.
*/
const byte PIN_PS2_ATT = 10;
const byte PIN_PS2_CMD = 11;
const byte PIN_PS2_DAT = 12;
const byte PIN_PS2_CLK = 13;

const byte PIN_BUTTONPRESS = A0;

const unsigned long POLLING_INTERVAL = 1000U / 400U;//needs more testing

// Send debug messages to serial port
//~ #define ENABLE_SERIAL_DEBUG
//#define ENABLE_SERIAL_DEBUG

PsxControllerBitBang<PIN_PS2_ATT, PIN_PS2_CMD, PIN_PS2_DAT, PIN_PS2_CLK> psx;

#ifdef ENABLE_SERIAL_DEBUG
//	#define dstart(spd) do {Serial.begin (spd); while (!Serial) {digitalWrite (LED_BUILTIN, (millis () / 500) % 2);}} while (0);
  #define dstart(spd) do {Serial.begin (spd); while (!Serial) {digitalWrite (PIN_BUTTONPRESS, (millis () / 500) % 2);}} while (0);

	#define debug(...) Serial.print (__VA_ARGS__)
	#define debugln(...) Serial.println (__VA_ARGS__)
#else
	#define dstart(...)
  #define debug(...)
  #define debugln(...)
#endif

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

boolean enableMouseMove = false;

word convertRange(word gcMin, word gcMax, word value)
{
  word scale = (word)(maxMouseValue / (gcMax - gcMin));
  return (word)((value - gcMin) * scale);
}

void releaseAllButtons () {
  AbsMouse.release(MOUSE_LEFT);
  AbsMouse.release(MOUSE_RIGHT);
  AbsMouse.release(MOUSE_MIDDLE);
  AbsMouse.report();
}


void setup () {
	dstart (115200);
	debugln (F("Ready!"));
}

void loop () {
	static unsigned long last = 0;
	
	if (millis () - last >= POLLING_INTERVAL) {
		last = millis ();
		
		if (!haveController) {
			if (psx.begin ()) {
				debugln (F("Controller found!"));
       
				haveController = true;
			}
		} else {
      noInterrupts ();
      boolean isReadSuccess = psx.read ();
      interrupts ();
      
			if (!isReadSuccess) {
				//debugln (F("Controller lost :("));
        debug (F("Controller lost."));
        debug (F(" last values: x = "));
        debug (lastX);
        debug (F(", y = "));
        debugln (lastY);
        
				haveController = false;
			} else {
        // Read was successful, so let's make up data for Mouse
        
        word x, y;
        GunconStatus gcStatus;

        if (!enableMouseMove && psx.buttonJustPressed (PSB_CIRCLE))
          enableMouseMove = true;
        
        if(psx.buttonJustPressed (PSB_CIRCLE)) //trigger press
          AbsMouse.press(MOUSE_LEFT);
        else if (psx.buttonJustReleased (PSB_CIRCLE)) //trigger release
          AbsMouse.release(MOUSE_LEFT);
       
        if(psx.buttonJustPressed (PSB_START)) //A button press
          AbsMouse.press(MOUSE_RIGHT);
        else if (psx.buttonJustReleased (PSB_START)) //A button release
          AbsMouse.release(MOUSE_RIGHT);

        if(psx.buttonJustPressed (PSB_CROSS)) //B button press
          AbsMouse.press(MOUSE_MIDDLE);
        else if (psx.buttonJustReleased (PSB_CROSS)) //B button release
          AbsMouse.release(MOUSE_MIDDLE);

        gcStatus = psx.getGunconCoordinates (x, y);
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
  
            if(enableMouseMove) {
              AbsMouse.move(convertRange(minX, maxX, x), convertRange(minY, maxY, y));
            }
          }
        } else if (gcStatus == GUNCON_NO_LIGHT){
          
          //up to 10 no_light reads will report the last good values
          if (lastX != 0 && lastY != 0) {
            AbsMouse.move(convertRange(minX, maxX, lastX), convertRange(minY, maxY, lastY));
            noLightCount++;
            
            if (noLightCount > maxNoLightCount) {
              noLightCount = 0;
              lastX = 0;
              lastY = 0;

              //set it offscreen (bottom left). need to test
              //also release all buttons
              AbsMouse.move(0, maxMouseValue);
              releaseAllButtons ();
            }
          }
          else if (psx.buttonPressed (PSB_CIRCLE) && psx.buttonPressed (PSB_START) && psx.buttonPressed (PSB_CROSS)){
            enableMouseMove = false;
            releaseAllButtons ();
          }

        }

        if(enableMouseMove){
          AbsMouse.report();
        }

			}
		}
	}
}
