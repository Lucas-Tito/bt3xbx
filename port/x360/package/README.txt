Dragon Ball Z: Budokai Tenkaichi 3 for Xbox 360
Version {{VERSION}}
================================================

1. WHAT YOU NEED
----------------

* Xbox 360 that can run .xex files.
* Your own ISO of the USA release, SLUS-21678.

    Size:    3,020,816,384 bytes
    MD5:     350404a81de912347c8b262542fed970
    SHA256:  93f1f911e9a2bbdf92e5794fc3075ddf3f87db3df4b1cb8cfbb0448721c91172

* 10 GB of free space.

2. INSTALLING
-------------

1. Copy the whole bt3-x360 folder to the console, to a place your dashboard scans for games,
   for example Hdd1:\Games\bt3-x360.
2. Copy your original USA ISO into that same folder, next to default.xex, and name it
   bt3.iso. Any name ending in .iso works, except bt3-x360.iso: that name is reserved for the
   converted ISO.
   (If you converted on a PC with the PC tool, copy your bt3-x360.iso there instead and skip
   step 3.)
3. The game asks to convert your ISO. Choose Convert (section 3, option A).

After the conversion the game folder looks like this:

    default.xex
    default.dat
    default.swap
    bt3-x360.iso     the converted ISO (the game uses this one)
    bt3.iso          your original (you can delete it from the console, see below)
    saves\           created by the game the first time it saves


3. CONVERTING YOUR ISO (TWO WAYS)
---------------------------------

The game cannot use the original ISO directly. It needs a converted copy: the game's data
reordered for the Xbox 360 processor, and the button pictures changed to Xbox 360 buttons.
The converted ISO is the same size as the original. Your original ISO is only read, never
changed. Pick one of the two ways below; both give a converted ISO that the game accepts.

Option A: on the console

1. With your original ISO in the game folder (section 2), start the game.
2. A box appears:

     Convert ISO
     Convert your original USA ISO for Xbox 360? It needs 3 GB of free space. Your original
     will be kept.

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

For Linux. You need Python 3.8 or newer and gcc. Go to the pc-tools folder of this package
and run:

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


4. SAVES
--------

The game saves as it did on the memory card, through its own menus. The files go to the saves
folder inside the game folder (saves\card1\BASLUS-21678DBZT3). To back them up, copy the saves
folder to your PC while the game is not running.

Saves from the PlayStation 2, from PCSX2 or from the PC port cannot be used here, and saves
from here cannot be used there. Importing and exporting saves is not part of this version.


5. IF THE GAME CRASHES
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


6. KNOWN ISSUES
---------------

* Night stages, the desert at night above all, can look too dark. I'm not sure yet whether
  this comes from the game or from the TV setup. The console's "Reference Levels" video
  setting (System Settings, Console Settings, Display, Reference Levels) must match the range
  your TV expects; when they differ, dark tones get darker. Try the other options there and
  compare.
* On Namek, during the dragon (Shenlong) animation, parts of the sky and the water can turn
  black for a moment.
* Not every character, costume, transformation, stage, story battle and mode has been played
  on the console for this version. If one of them crashes, please report it (section 5).
* The online mode (Dragon Net Battle) is not available.
