# Reporting problems

This is the Xbox 360 port of Dragon Ball Z: Budokai Tenkaichi 3. Issues are for crashes and bugs of
this port. Everything you need to install and play is in
[README.txt](https://github.com/Lucas-Tito/bt3xbx/blob/main/port/x360/package/README.txt); please
read it, and its known issues, before opening an issue.

## Known issues: please do not report these

These are already known and listed in section 6 of README.txt. An issue about one of them is closed
with the label `known-issue`.

* Night stages, the desert at night above all, can look too dark (check the console's Reference
  Levels video setting first, see README.txt).
* On Namek, during the Shenlong animation, parts of the sky and the water can turn black for a
  moment.
* The planet explosion animation runs slowly for about 3 seconds, and split-screen battles can
  slow down.
* The online mode (Dragon Net Battle) is not available.
* On consoles running Bad Avatar, the system's message boxes (missing or unreadable ISO, the
  crash box) can close by themselves. The ISO conversion question is not affected since 1.0.1.

Not every character, stage, story battle and mode has been played on the console yet: a crash or a
problem in one of them is not a known issue, so please do report it.

## How to report

* **The game closed with the "Unexpected error" box:** use the **Crash report** form and attach the
  `crash.txt` file from the game folder (copy it before starting the game again).
* **Something works wrong without a crash:** use the **Bug report** form.
* One problem per issue. Search the existing issues first; if yours is there, add your details to
  it instead of opening a new one.
* Fill in every field. The version, how you converted the ISO, the mode, the characters and the
  stage are what makes a report possible to follow.

Blank issues are turned off: every issue comes from one of the two forms.

## What is not accepted

These issues are closed (or deleted) without an answer:

* **Asking for the game, an ISO, or links to them, or posting such links.** This project never
  provides game files. Deleted at once.
* **Other versions of the game.** Only the USA release, SLUS-21678, unmodified, is supported. PAL,
  Japanese, the Wii version, fan projects built on the game (such as Budokai Tenkaichi 4), patched
  or modified ISOs, and texture or character mods are not supported.
* **Consoles without RGH or JTAG, and emulators.** The port is made for a modified Xbox 360
  running unsigned code.
* **Duplicates.** Closed with a link to the first report.
* **A crash report without `crash.txt`.** Closed automatically at once, with the label
  `needs-crash-txt`. Attach the file (or paste its content) by editing the issue or in a comment,
  and it reopens by itself.
* **Reports with fields left empty or filled with nonsense,** and reports of something already in
  the known issues of README.txt.
* **Problems of the PC version.** They belong to
  [Tenkaichi3Decomp](https://github.com/z3xox/Tenkaichi3Decomp/issues).
* **Requests for new features.** This first version keeps the game as it is; requests are not
  tracked as issues for now.

## What happens to those who break the rules

1. The issue gets a label saying why (`duplicate`, `not-supported`, `off-topic`, `known-issue`,
   `feature-request`); a short reply explaining it is posted automatically and the issue is closed.
2. Repeating it after being pointed to these rules, or being rude, gets the conversation locked.
3. Anything about getting the game (`piracy`), spam or abuse is deleted, and the account is
   blocked from the repository.

Offensive language, insults and advertising are caught automatically: an issue with them is
closed and locked with the label `spam`, and a comment with them is hidden. Keep it civil, even
when the game crashes.

## Labels

The full list, with colours and meanings, is in [labels.yml](labels.yml): `crash`, `bug`,
`needs-triage`, `needs-crash-txt`, `needs-info`, `confirmed`, `known-issue`, `fixed-next`,
`duplicate`, `not-supported`, `piracy`, `off-topic`, `feature-request`, `wontfix` and `spam`.

## Code

Never add game files, or anything taken from the game, to a commit: no ISO and no data extracted
from it.
