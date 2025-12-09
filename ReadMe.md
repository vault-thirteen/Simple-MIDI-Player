# Simple MIDI Player

This player is able to play MIDI files using `DirectSound` API and `WinMM` library.  
`DirectSound` API can be used for playback on all synthesizers.  
`WinMM` library is able to play MIDI files only on external software and hardware synthesizers.  

This project was created and configured in a Microsoft Visual Studio 2010.  
In order to compile this project a set of conditions must be met:
* Visual Studio 2010 must have all the latest patches, including Service Pack 1 and hotfixes released after it.
* Windows SDK 7.1 is required to build the project.
* Microsoft DirectX SDK (dated August 2007) is strictly required.
* A strict order of includes is also required.

### Why ?

* Later versions of DirectX SDK do not contain library and header files for `DirectSound` API.
* Windows SDK released after Windows 7 are not compatible with `DirectSound` API.
* Modern versions of Visual Studio, such as Visual Studio 2022, can be used for editing the source code and 
  compiling the project, but it is required that the project is created in the old Visual Studio 2010, because 
  otherwise you will see the hell.

Notes on installing the required software and configuring the environment are stated in the text files:
* [DirectSound API / Direct Sound API.txt](<DirectSound API/Direct Sound API.txt>)
* [DirectSound API / Building for DirectSound API.txt](<DirectSound API/Building for DirectSound API.txt>)
* [Visual Studio 2010 / How to install.txt](<Visual Studio 2010/How to install.txt>)
* [Visual Studio 2010 / Versions.txt](<Visual Studio 2010/Versions.txt>)

## Usage
To see a help information simply start the player in a command prompt without any arguments.

```
Usage:
        <executable> <Work mode> ...

Number of arguments depends on the work mode set as a first argument in the command line.
Available word modes are:
         DS - This mode uses DirectSound API;
         MM - This mode uses WinMM library.

Arguments (4) for DirectSound mode are:
        <DirectSound device index> <MIDI output device index> <DLS file> <MIDI file>
Arguments (2) for WinMM mode are:
        <Port number / Device ID> <MIDI file>

Notes for DirectSound mode:
        Set the DirectSound device index to a negative value to use the default device.
        Set the MIDI output device index to a negative value to use the default device.
        To disable loading DLS, use the '-' as DLS file.
        This mode has a known problem. When a default MIDI output is selected (i.e. negative index), the DirectSound API initialises automatically and maps MIDI channels incorrectly. Automatic initialisation does not allow manual channel mapping. Incorrect channel mapping results in most of the instruments lost and quiet. All this means that you should not use the default MIDI output.

Notes for WinMM mode:
        Do not use this mode for playing MIDI files on a Microsoft's software synthesizer, also known as Microsoft GS Wavetable Synth. This mode is used mostly for software and hardware synthesizers present on your sound card or for external hardware synthesizers.

Examples:
        tool.exe DS -1 0 gm.dls music.mid
        tool.exe DS -1 0 - music.mid
        tool.exe MM 1 music.mid
```

A screenshot of a command prompt with the help information can be seen here: 
[Screenshot/usage.png](Screenshot/usage.png)

When started without arguments, the tool shows a list of all available output sound devices visible to `DirectSound` 
API, a list of MIDI ports visible by `DirectSound` and a list of MIDI Out devices reachable by `WinMM` library. 

## Capabilities
In the `MM` work mode, the player uses all the power of the ancient `WinMM` library. Despite its very old age, this 
library is still capable of playing MIDI files. This mode is used mostly for playback on external synthesizers.

In the `DS` work mode, the player uses those remnants of the `DirectSound` API which Microsoft has not yet destroyed. 
This mode can be used for playback on almost all synthesizers. This mode allows to use a custom DLS file by 
specifying its path or playing MIDI music with the help of default `gm.dls` file present in modern 
Windows 10 operating systems. If you need this file for some reason, it normally sits in the follwoing folders:
* `C:\Windows\System32\drivers`
* `C:\Windows\SysWOW64\drivers`

If you need to provide a custom sound font (SF2 file) to a MIDI synthesizer, then you should use a tool more advanced 
than this player, because this player is very simple and performs only basic functions.
