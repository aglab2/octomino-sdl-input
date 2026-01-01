Octomino's SDL Input Plugin
===========================
An input plugin for Zilmar spec emulators including Project 64 1.6. It 
uses SDL3's Gamepad API and supports many gamepads including Xbox, 
PS4, and Switch Pro.

This is a fork of [clickdevin/octomino-sdl-input](https://github.com/clickdevin/octomino-sdl-input), focused on adding a configuration GUI as well as new features.

Downloading
-----------
The latest release can be downloaded from 
[here](https://github.com/Luna-Project64/octomino-sdl-input/releases).

Installing
----------
Copy `octomino-sdl-input.dll` and `gamecontrollerdb.txt` to your 
emulator's plugins folder.

Building from source
--------------------
Make sure to check out the repository with submodules:
```
git clone --recursive https://github.com/Luna-Project64/octomino-sdl-input
```
Or, if you have cloned the repository already:
```
git submodule update --init
```

Visual Studio 2022 toolchain with clang-cl is used for building. Either open the .sln in the IDE, or open the "Developer Command Prompt for VS2022" and run `msbuild`.

License
-------
This plugin is licensed under the Mozilla Public License 2.0. See 
`LICENSE` or visit <http://mozilla.org/MPL/2.0/>.

Third party libraries used:
* [microui by rxi](https://github.com/rxi/microui) (MIT, see `src/microui.h` for details)
* [ini.h by Mattias Gustavsson](https://github.com/mattiasgustavsson/libs/blob/main/ini.h) (MIT, see `src/ini.h` for details)
