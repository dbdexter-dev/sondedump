Radiosonde decoder
==================

This is a free, open-source radiosonde decoder and tracker. Given an
FM-demodulated signal (either in .wav form or directly fed from an audio
source), it will decode it and output the recovered data in different formats.

Features:
- Decoding of calibrated PTU data (pressure, temperature, humidity)
- Decoding of GPS position and velocity
- Burstkill timer indication
- GPX and KML output
- Live KML output (realtime position in Google Earth)
- Read FM-demodulated data directly from audio (requires `portaudio`)

Supported models:
- RS41-SG(P)

Build/install instructions
--------------------------
Optional libraries:
- `ncurses`: Simple TUI with data summary
- `portaudio`: Live streaming of samples from audio device

Compilation:
```
mkdir build && cd build
cmake ..
make
```

Usage
-----
```
sondedump [options] file_in
If no filename is specified and PortAudio support has been enabled during
compilation, samples will be read from an audio device instead.

   -a, --audio-device <id> Use PortAudio device <id> as input (default: choose interactively)
   -c, --csv <file>        Output data to <file> in CSV format
   -f, --fmt <format>      Format output lines as <format>
   -g, --gpx <file>        Output GPX track to <file>
   -k, --kml <file>        Output KML track to <file>
   -l, --live-kml <file>   Output live KML track to <file>

   -h, --help              Print this help screen
   -v, --version           Print version info

Available format specifiers:
   %a      Altitude (m)
   %b      Burstkill/shutdown timer
   %c      Climb rate (m/s)
   %d      Dew point (degrees Celsius)
   %f      Frame counter
   %h      Heading (degrees)
   %l      Latitude (decimal degrees + N/S)
   %o      Longitude (decimal degrees + E/W)
   %p      Pressure (hPa)
   %r      Relative humidity (%)
   %s      Speed (m/s)
   %S      Sonde serial number
   %t      Temperature (degrees Celsius)
   %T      Timestamp (yyyy-mm-dd hh:mm::ss, local)

```

Examples:
- Use Portaudio device 1 and output to CSV: `sondedump --audio-device 1 --csv
  data.csv`
- Read from file and generate GPX track: `sondedump --gpx track.gpx
  <recording.wav>`
