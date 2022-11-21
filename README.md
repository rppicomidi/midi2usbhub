# midi2usbhub
Use a Raspberry Pi Pico to interconnect MIDI devices via a USB hub or old school MIDI.

This project uses a Pico board, a micro USB to USB A adapter, and a powered USB hub
to run software that routes MIDI data among all the devices connected to the hub. You
configure the routing with command line interpreter commands through a serial port
terminal. The software uses some of the Pico board's program flash for a file system
to store configurations in presets. You can back up any or all of your presets to a
USB Flash drive connected to the USB hub. Presets are stored in JSON format. There
is a UART DIN MIDI IN and a UART DIN MIDI OUT to, so you can connect to old schoold
MIDI too.

# Project Status
Very early public release to help with USB MIDI host hub testing. Definitely not done.
May crash from time to time when you plug in a new device. I have not investigated that
yet. Not easily repeatable. UART MIDI is also implemented.

## Commands supported:
- list
- connect
- disconnect
- rename
- reset
- show
- help

## TODO
- local settings storage with LittleFs
- backup and restore to flash drive

## Future Feature
- Implement on a Pico-W with embedded web server support so you don't need to use
the CLI.

# Setting Up Your Build and Debug Environment
I am running Ubuntu Linux 20.04LTS on an old PC. I have Visual Studio Code (VS Code)
installed and went
through the tutorial in Chapter 7 or [Getting started with Raspberry Pi Pico](https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf) to make sure it was working
first. I use a picoprobe for debugging, so I have openocd running in a terminal window.
I use minicom for the serial port terminal (make sure your linux account is in the dialup
group).

## Using the forked tinyusb library
The Pico SDK uses the main repository for tinyusb as a git submodule. Until the USB Host driver for MIDI is
incorporated in the main repository for tinyusb, you will need to use the latest development version in pull
request 1627 forked version. This is how I do it.

1. If you have not already done so, follow the instructions for installing the Raspberry Pi Pico SDK in Chapter 2 of the 
[Getting started with Raspberry Pi Pico](https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf)
document.
2. Set the working directory to the tinyusb library
```
cd [some directory where you installed the pico SDK]/lib/tinyusb
```
3. Get the pull request midihost branch
```
git fetch origin pull/1627/head:pr-midihost
```
4. Checkout the branch
```
git checkout pr-midihost
```
## Get the project code
Clone the midiusb2hub project to a directory at the same level as the pico-sdk directory.

```
cd [one directory above the pico-sdk directory]
git clone --recurse-submodules https://github.com/rppicomidi/midi2usbhub.git
```
## Command Line Build (skip if you want to use Visual Studio Code)

Enter this series of commands (assumes you installed the pico-sdk
and the midid2usbhub project in the $HOME/foo directory)

```
export PICO_SDK_PATH=$HOME/foo/pico-sdk/
cd $HOME/foo/midi2usbhub
mkdir build
cd build
cmake ..
make
```
The build should complete with no errors. The build output is in the build directory you created in the steps above.

# Terms this document uses
- **Connected MIDI Device**: a MIDI device connected to a USB hub port or to a serial
port MIDI DIN connector.
- **USB ID**: A pair of numbers the Connected MIDI Device reports to the
hub when it connects. They are supposed to be unique to a particular
product.
- **Routing Matrix**: The software that sends MIDI data to and from Connected MIDI Devices
- **Terminal**: a MIDI data input to or output from the Routing Matrix.
- **FROM terminal**: an input to the Routing Matrix. It will be a MIDI OUT signal from
a Connected MIDI Device.
- **TO terminal**: an output from the Routing Matrix. It will be a MIDI IN signal to
a Connected MIDI Device.
- **Port**: usually a group of 1 MIDI IN data stream and one MIDI OUT data stream associated with a Connected MIDI Device. A Port of a Connected MIDI Device may omit MIDI IN or MIDI OUT, but not both. Ports are numbered 1-16
- **Direction** of a terminal: either FROM the Connected MIDI Device's MIDI OUT
or TO the Connected MIDI Device's MIDI IN.
- **Nickname**: a more human name than specifying a device port's FROM and TO
data streams using a USB ID, a Port number and a Direction. Nicknames have
a maximum of 12 characters. The default nickname for a port in a given
direction is the USB ID followed by either a "F" for a FROM data stream or
"T" for a TO data stream, followed by the port number (1-16). For example,
"Drumpads" above was renamed from "0000-0000-F1"
- **Product Name**: a name that identifies the the attached MIDI
device. The Connected MIDI Device sends it to the hub on connection; it is a more friendly
name than USB ID, and is the easiest way to assocate the Connected MIDI Device
with all the other info.

# Command Line Commands
## help
Show a list of all available commands and brief help text.

## list
List all Connected MIDI Devices currently connected to the USB hub. For example:

```
USB ID      Port  Direction Nickname    Product Name
0000-0000    1      FROM    Drumpads    MIDI IN A
0000-0000    1       TO     TR-707      MIDI OUT A
0499-1622    1      FROM    lead-out    reface CS
0499-1622    1       TO     lead        reface CS
1C75-02CA    1      FROM    keys        Arturia Keylab Essential 88
1C75-02CA    1       TO     keys-in     Arturia Keylab Essential 88
1C75-02CA    2      FROM    faders      Arturia Keylab Essential 88
1C75-02CA    2       TO     faders-in   Arturia Keylab Essential 88
```

## rename \<Old Nickname\> \<New Nickname\>
Rename the nickname for a product's port. All nicknames must be unique. If you need to
hook up more than one device with the same USB ID, then you must do so one at a
time and change the nickname for each port before attaching the next one to the hub.

## connect \<From Nickname\> \<To Nickname\>
Send data from the MIDI Out port of the MIDI device with nickname \<From Nickname\> to the
MIDI IN port of the device with nickname \<To Nickname\>. If more than one device connects
to the TO terminal of a particular device, then the streams are merged.

## disconnect \<From Nickname\> \<To Nickname\>
Break a connection previously made using the `connect` command.

## reset
Disconnect all routings. 

## show
Show a connection matrix of all MIDI devices connected to the hub. A blank box means "not
connected" and an `x` in the box means "connected." For example, the following shows
MIDI OUT of the "keys" device connected to the MIDI IN of the "lead" device.

```
       TO-> |   |   |   |
            |   |   |   |
            |   |   |   |
            |   |   | f |
            |   |   | a |
            | l | k | d |
            | e | e | e |
FROM |      | a | y | r |
     v      | d | s | s |
            | - | - | - |
            | i | i | i |
            | n | n | n |
------------+---+---+---+
lead        |   |   |   |
------------+---+---+---+
keys        | x |   |   |
------------+---+---+---+
faders      |   |   |   |
------------+---+---+---+
```

## save \<preset name\>
Save the current setup to the given \<preset name\>. If there is already a preset with that
name, then it will be overwritten.

## load \<preset name\>
Load the current setup from the given \<preset name\>. If the preset was not previously
saved using the save command, then print an error message to the console.

## backup [\<preset name\>]
Copy the specified preset to USB flash drive to a file on the drive named `/rppicomidi-midi2usbhub/<preset name>`. If no preset name is given, then all presets are copied to the
flash drive.

## restore [\<preset name\>]
Copy the specified preset from the USB flash drive directory `/rppicomidi-midi2usbhub/<preset name>` to the file system on Pico board's program flash. If no preset name is specified, then
all presets from the USB flash drive directory `/rppicomidi-midi2usbhub` will be copied.

