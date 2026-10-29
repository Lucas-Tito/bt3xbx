Dragon Ball Z: Budokai Tenkaichi 3 for Xbox 360
Version {{VERSION}}
================================================

A native Xbox 360 port of Dragon Ball Z: Budokai Tenkaichi 3 (PlayStation 2, USA release,
SLUS-21678). It is the game's own code, rebuilt for the Xbox 360 from a decompilation. It is
not an emulator.

This package contains NO game data. You need your own disc image (ISO) of the USA release of
the game. Nothing in this project helps you get one: make it from a disc you own.

This project is not affiliated with or endorsed by the game's developers, publishers or rights
holders. All rights to the game belong to their owners.


CONTENTS
--------

  1. What you need
  2. What is in this package
  3. Installing
  4. Converting your ISO (two ways)
  5. Updating to a new version
  6. Controls
  7. Saves
  8. If the game crashes
  9. Known issues
  10. Credits, licence and legal


1. WHAT YOU NEED
----------------

* An Xbox 360 with RGH or JTAG that can run unsigned homebrew (.xex files). A retail console
  that is not modified cannot run this.
* Your own ISO of the USA release, SLUS-21678. Other regions (PAL, Japan) and other versions
  are not supported. You can check your ISO on a PC:

    Size:    3,020,816,384 bytes
    MD5:     350404a81de912347c8b262542fed970
    SHA256:  93f1f911e9a2bbdf92e5794fc3075ddf3f87db3df4b1cb8cfbb0448721c91172

* Free space on the drive where the game goes: 3 GB for the converted ISO, plus the original
  ISO (another 3 GB) while you convert. About 6 GB in total.
* A controller, and a way to copy files to the console (FTP from your dashboard, or a USB
  drive).

What was tested: Xbox 360 E, RGH, kernel 2.0.17150, FreeStyle Dash 3.0.780 with DashLaunch,
the game on the internal hard drive. Not tested yet: Aurora and other dashboards, other kernels,
USB drives. If you try one of those, a report helps (section 8).


2. WHAT IS IN THIS PACKAGE
--------------------------

  bt3-x360\            The game folder. This is what goes to the console.
    default.xex        The game.
    default.dat        Tables the game needs. Always keep these three files together and
    default.swap       from the same version.
  pc-tools\            Optional: the ISO converter for a PC (section 4, option B).
  README.txt           This file.
  LICENSE.txt          The licence of the port's code.
  THIRD-PARTY.txt      Licences and credits of the parts made by others.
  licenses\            The full licence texts named in THIRD-PARTY.txt.
  SHA256SUMS.txt       Checksums of every file above.

To check that your download is complete, run this in the unpacked folder on Linux or WSL:

    sha256sum -c SHA256SUMS.txt

Every line must end with "OK".


3. INSTALLING
-------------

1. Unpack the zip on your PC.
2. Copy the whole bt3-x360 folder to the console, to a place your dashboard scans for games,
   for example Hdd1:\Games\bt3-x360.
3. Copy your original USA ISO into that same folder, next to default.xex, and name it
   bt3.iso. Any name ending in .iso works, except bt3-x360.iso: that name is reserved for the
   converted ISO.
   (If you converted on a PC, copy your bt3-x360.iso there instead and skip step 5.)
4. Start "Budokai Tenkaichi 3" from your dashboard. If it is not listed, rescan your game
   folders, or start default.xex from the dashboard's file manager.
5. The game asks to convert your ISO. Choose Convert (section 4, option A).

After the conversion the game folder looks like this:

    default.xex
    default.dat
    default.swap
    bt3-x360.iso     the converted ISO (the game uses this one)
    bt3.iso          your original (you can delete it from the console, see below)
    saves\           created by the game the first time it saves


4. CONVERTING YOUR ISO (TWO WAYS)
---------------------------------

The game cannot use the original ISO directly. It needs a converted copy: the game's data
reordered for the Xbox 360 processor, and the button pictures changed to Xbox 360 buttons.
The converted ISO is the same size as the original. Your original ISO is only read, never
changed. Pick one of the two ways below; both give a converted ISO that the game accepts.

Option A: on the console (recommended)

1. With your original ISO in the game folder (section 3), start the game.
2. A box appears:

     Convert ISO
     Convert your original USA ISO for Xbox 360? This takes up to 40 minutes and needs 3 GB of
     free space. Your original will be kept.

   Cancel is selected first. Move to Convert and press A.
3. A progress screen shows the step (Checking ISO..., Copying ISO..., Converting ISO...,
   Verifying ISO..., Finishing...), a bar and a percentage, with the line "Please wait. Do not
   turn off the console." The console reads the whole ISO, writes the converted one and then
   reads it back to make sure every byte was stored correctly.
4. At the end:

     ISO ready
     Conversion complete. The game will restart.

   Press OK. The game restarts by itself and from now on starts normally.

If something goes wrong:

* "Not enough space": free at least 3 GB on the drive where the game is, and start again.
* "Conversion failed": the ISO is not the unmodified USA release, or the drive had a problem.
  Check your ISO against the size and checksums in section 1, copy it again and retry.
* "Rename your original ISO to bt3.iso and try again...": your original is named
  bt3-x360.iso. Rename it to bt3.iso.
* If the console turns off or you leave during the conversion, nothing is lost: the next start
  removes the unfinished file (bt3-x360.tmp) and asks again from the beginning.

After a successful conversion you can delete the original ISO from the console to get the
space back. Keep a copy on your PC: a future version may need a new conversion, and that one
also starts from the original.

Option B: on a PC (advanced)

For Linux, or Windows with WSL. You need Python 3.8 or newer and gcc. Go to the pc-tools
folder of this package and run:

    mkdir -p port/build
    gcc -std=c89 -O2 -o port/build/conv_tool $(ls port/x360/conv/*.c | grep -v conv_jogo.c)
    python3 port/x360/tools/convert_iso.py /path/to/your/original.iso /path/to/bt3-x360.iso

The first two lines build the converter (conv_tool) once. The last line checks that the ISO
is the unmodified USA release (size, MD5 and SHA256), converts it, then reads the result again
and compares every byte. It needs about 6 GB free in the folder of the output during the work
(3 GB stay at the end) and takes several minutes. When it ends well, the last line is
"all done". On any error it prints a line starting with "convert_iso:", exits with code 1
and does not leave a bt3-x360.iso behind.

Then copy bt3-x360.iso into the game folder on the console, next to default.xex. You do not
need to copy the original.


5. UPDATING TO A NEW VERSION
----------------------------

Replace default.xex, default.dat and default.swap together, all three from the new package.
Keep the saves folder and your converted ISO. If the new version needs a new conversion, the
game says so when it starts; put your original ISO back in the folder and convert again
(section 4).


6. CONTROLS
-----------

The game's own controls, with the Xbox 360 buttons in the places of the PlayStation 2 buttons.
The pictures of buttons in the game (menus, lessons, command lists) show the Xbox 360 buttons.

    Xbox 360              PlayStation 2
    A                     Cross
    B                     Circle
    X                     Square
    Y                     Triangle
    LB / RB               L1 / R1
    LT / RT               L2 / R2
    Left stick click      L3
    Right stick click     R3
    Start                 Start
    Back                  Select
    D-pad, sticks         D-pad, sticks

* Y goes back in the menus, as Triangle did on the PlayStation 2.
* Player 1 is the first controller that is turned on; player 2 is the next one.
* Vibration works as in the original game, and can be turned off in the game's options.
* The controller layout can be changed in the game's own options, as on the PlayStation 2.
  This version has no other remapping.
* The game has no quit option. To leave, press the Guide button and go back to the dashboard.


7. SAVES
--------

The game saves as it did on the memory card, through its own menus. The files go to the saves
folder inside the game folder (saves\card1\BASLUS-21678DBZT3). To back them up, copy the saves
folder to your PC while the game is not running.

Saves from the PlayStation 2, from PCSX2 or from the PC port cannot be used here, and saves
from here cannot be used there. Importing and exporting saves is not part of this version.


8. IF THE GAME CRASHES
----------------------

If the game runs into an error, it shows:

    Unexpected error
    The game ran into an unexpected error and needs to close. A report was saved to crash.txt
    in the game folder. Press OK to return to the dashboard.

Press OK. Then, before starting the game again:

1. Copy crash.txt from the game folder to your PC (FTP or USB). It holds only technical data
   about the game at the moment of the error (the version, where in the code it stopped and
   the processor's state). No personal data.
2. Open an issue on the project's GitHub page with the "Crash report" form:

     https://github.com/Lucas-Tito/bt3xbx/issues/new/choose

   Attach crash.txt and fill in every field: the version ({{VERSION}}), what you were doing,
   the mode, the characters and the stage, and whether you converted the ISO on the console or
   on the PC. Without crash.txt a crash cannot be looked into, and the issue is closed.
3. One crash per issue. Search the open issues first: if yours is already there, add your
   crash.txt to it instead of opening a new one.

A problem that is not a crash (a wrong picture, wrong sound, a menu that does not work) goes in
the "Bug report" form. Please read the known issues below first.


9. KNOWN ISSUES
---------------

* Night stages, the desert at night above all, can look too dark. We are not sure yet whether
  this comes from the game or from the TV setup. The console's "Reference Levels" video
  setting (System Settings, Console Settings, Display, Reference Levels) must match the range
  your TV expects; when they differ, dark tones get darker. Try the other options there and
  compare.
* The picture is always 16:9 in this version, also on a TV set to 4:3, where it looks
  stretched. Menus and movies are stretched to 16:9, as in the PC port. Following the
  console's 4:3 or widescreen setting is planned for a later version.
* In 16:9 the gauges and other on-screen displays are narrowed and look a little blurry.
* On Namek, during the dragon (Shenlong) animation, parts of the sky and the water can turn
  black for a moment.
* Not every character, costume, transformation, stage, story battle and mode has been played
  on the console for this version. If one of them crashes, please report it (section 8).
* The online mode (Dragon Net Battle) is not available.
* There is no settings menu of the port, no button remapping beyond the game's own options, and
  no import or export of saves.


10. CREDITS, LICENCE AND LEGAL
------------------------------

* Tenkaichi3Decomp, the native PC port this one is built on, by z3xox and contributors:
  https://github.com/z3xox/Tenkaichi3Decomp
* BT3-Decompiled, the decompilation of the game: https://github.com/z3xox/BT3-Decompiled
* Xbox 360 port (bt3xbx): Lucas Tito, https://github.com/Lucas-Tito/bt3xbx
* Xbox 360 button pictures: "Input Prompts" by Kenney (www.kenney.nl), public domain (CC0).
  Thank you, Kenney.

The port's own code is under the MIT licence (LICENSE.txt). The parts made by others keep their
own licences (THIRD-PARTY.txt). The game's code comes from the decompilation and is not ours to
license: it belongs to the game's rights holders.

This project is not affiliated with or endorsed by the game's developers, publishers or rights
holders. It exists for preservation, study and interoperability, and distributes none of the
game's assets. All rights to the game belong to their owners. You need your own legally
obtained copy of the game to use it.
