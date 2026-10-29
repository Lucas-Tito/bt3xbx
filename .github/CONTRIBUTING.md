# Reporting problems

This is the Xbox 360 port of Dragon Ball Z: Budokai Tenkaichi 3. Issues are for crashes and bugs of
this port. Everything you need to install and play is in
[README.txt](https://github.com/Lucas-Tito/bt3xbx/blob/main/port/x360/package/README.txt); please
read it, and its known issues, before opening an issue.

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
* **A crash without `crash.txt`.** Marked `needs-crash-txt` and closed if the file does not arrive
  in 14 days.
* **Reports with fields left empty or filled with nonsense,** and reports of something already in
  the known issues of README.txt.
* **Problems of the PC version.** They belong to
  [Tenkaichi3Decomp](https://github.com/z3xox/Tenkaichi3Decomp/issues).
* **Requests for new features.** This first version keeps the game as it is; requests are not
  tracked as issues for now.

## What happens to those who break the rules

1. The issue is closed with a label saying why (`duplicate`, `not-supported`, `needs-crash-txt`,
   `off-topic`).
2. Repeating it after being pointed to these rules, or being rude, gets the conversation locked.
3. Anything about getting the game (`piracy`), spam or abuse is deleted, and the account is
   blocked from the repository.

## Labels

The full list, with colours and meanings, is in [labels.yml](labels.yml): `crash`, `bug`,
`needs-triage`, `needs-crash-txt`, `needs-info`, `confirmed`, `known-issue`, `fixed-next`,
`duplicate`, `not-supported`, `piracy`, `off-topic` and `wontfix`.

## Code

Before a pull request, open an issue to talk about it. Never add game files, or anything taken
from the game, to a commit: no ISO and no data extracted from it.
