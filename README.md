# GunconDuino

PS1 Guncon controller as a Mouse via Arduino Leonardo

This is a work in progress project. It works as it is but I can't provide support.
Use at you own risk.

See it in action
https://www.youtube.com/watch?v=45CCB9uxqrk

Wire the controller directly to the arduino or use this [shield](https://github.com/SukkoPera/PsxControllerShield)

While developing I'm using a psx controller extension cable wired to the arduino
![device](docs/img01.jpg)

Before using it you will need the library PsxNewLib.
There's a dedicated [branch](https://github.com/SukkoPera/PsxNewLib/tree/guncon_support) with support for the gcon.

Also need to edit the file PsxNewLib.h and change INTER_CMD_BYTE_DELAY to 50

This only works on a CRT at sd resolutions (15K)

### Usage

When connecting it to a PC it will be in a not enabled state. It will not report any mouse movement.
To enable it simply press the trigger once.
If need to disable it, point the gun outside of the screen and press A + B + Trigger.

The guncon needs to "scan" the entire screen before it can properly send the coorinates.
Just point it at the screen and move slowly from side to side and top to botom.
It's recommended to use a full screen white image when doing this.

Buttons are mapped as follows:
* A (Left side) -> Mouse Right
* B (Right side) -> Mouse Middle
* Trigger -> Mouse Left
 
### Credits
This piece of software would not be possible without the amazing [PsxNewLib](https://github.com/SukkoPera/PsxNewLib).

It also uses a modified version of [absmouse](https://github.com/jonathanedgecombe/absmouse)
