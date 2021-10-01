Radiosonde decoder
==================

Still under heavy WIP, it's useable if you're a bit of a masochist, but if you
need docs you'll probably have to wait a bit

Features:
- Decoding of calibrated PTU data (pressure, temperature, humidity)
- Decoding of GPS position and velocity
- Burstkill timer indication
- GPX and KML output
- Live KML output (realtime position in Google Earth)
- Stuve diagram generation (requires `cairo`)

Supported models:
- RS41-SG(P)

Build/install instructions
--------------------------
Optional libraries:
- `cairo`: Thermodynamic diagram generation
- `ncurses`: Simple TUI with data summary

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
   -f, --fmt <format>      Format output lines as <format>
   -g, --gpx <file>        Output GPX track to <file>
   -k, --kml <file>        Output KML track to <file>
   -l, --live-kml <file>   Output live KML track to <file>
       --stuve <file>      Generate Stuve diagram and output to <file>

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
- Stuve diagram + live KML track: `sondedump --live-kml sonde-live.kml --stuve
  stuve.png <input file>`
- PTU data in CSV format: `echo "Temperature,Pressure,RH,Dewpoint" > data.csv;
sondedump --fmt "%t,%p,%r,%d" <input file> >> data.csv`
