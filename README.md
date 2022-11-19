# midi2usbhub
Use a Raspberry Pi Pico to interconnect MIDI devices via a USB hub

This project uses a Pico board, a micro USB to USB A adapter, and a powered USB hub
to run software that routes MIDI data among all the devices connected to the hub. You
configure the routing with command line interpreter commands through a serial port
terminal. The software uses some of the Pico board's program flash for a file system
to store configurations in presets. You can back up any or all of your presets to a
USB Flash drive connected to the USB hub. Presets are stored in JSON format.

A future version of this project will also support old school MIDI DIN connectors.

# Command Line Commands
## list
List all devices currently connected to the USB hub. For example:

```
USB ID      Port  Nickname Product Name
MMMM-DDDD    1    lead     refaceCS
MMMM-DDDD    1    keys     Arturia Keylab Essential 88
MMMM-DDDD    2    faders   Arturia Keylab Essential 88
```

## rename \<USB ID\> \<Port\> [\<Nickname\>]
Rename the nickname for a product using by specifying its USB-ID and its MIDI port.
Nicknames have a maximum of 8 characters. If the \<Nickname\> parameter is not
specified, then the nickname for the device port reverts to default, which is the
first 7 characters of the Product Name followed by followed by a number that
reflects the number of times the first 7 characters have been used (starting with 0).
In the very unlikely event there are more than 10 nicknames with the first 7 characters,
then the letters of the alphabet are used, upper case, starting with A.

## connect \<From Name\> \<To Name\>
Send data from the MIDI Out port of the MIDI device with nickname \<From Name\> to the
MIDI IN port of the device with nickname \<To Name\>. If more than one devices connect
to the MIDI IN port of a particular device, then the streams are merged.

## show \<Nickname\>
Show the devices, if any, connected to the MIDI IN port of the MIDI device with nickname
\<Nickname\>. For example:

```
MMMM-DDDD 1 lead refaceCS
    MMMM-DDDD 1 keys Arturia Keylab Essential 88
```

## matrix
Show a connection matrix of all MIDI devices connected to the hub. A blank box means "not
connected" and an `x` in the box means "connected." For example, the following shows
MIDI OUT of the "keys" device connected to the MIDI IN of the "lead" device.

```
   IN-> |   |   |   |
        |   |   |   |
        |   |   | f |
        |   |   | a |
        | l | k | d |
        | e | e | e |
OUT |   | a | y | r |
    v   | d | s | s |
        +---+---+---+
lead    |   |   |   |
        +---+---+---+
keys    | x |   |   |
        +---+---+---+
faders  |   |   |   |
        +---+---+---+
```

## save \<preset name\>
Save the current setup to the given \<preset name\>. If there is already a preset with that
name, then it will be overwritten.

## load \<preset name\>
Load the current setup from the given \<preset name\>. If the preset was not previously
saved using the save command, then print an error message to the console.

## reset \<routing|all\>
Clear the current routing if `routing` is specified. Clear routing and all nicknames if
`all` is specified.

## backup [\<preset name\>]
Copy the specified preset to USB flash drive to a file on the drive named `/rppicomidi-midi2usbhub/<preset name>`. If no preset name is given, then all presets are copied to the
flash drive.

## restore [\<preset name\>]
Copy the specified preset from the USB flash drive directory `/rppicomidi-midi2usbhub/<preset name>` to the file system on Pico board's program flash. If no preset name is specified, then
all presets from the USB flash drive directory `/rppicomidi-midi2usbhub` will be copied.

