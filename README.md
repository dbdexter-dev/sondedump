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

Compatibility matrix:

| Manufacturer | Model    | GPS                | Temperature        | Humidity           |
|--------------|----------|--------------------|--------------------|--------------------|
| Vaisala      | RS41-SG  | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark: |
| Meteomodem   | M10/M20  | :heavy_check_mark: |                    |                    |
| GRAW         | DFM06/09 | :heavy_check_mark: | :heavy_check_mark: |                    |
| Meisei       | iMS-100  | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark: |


Build/install instructions
--------------------------
No external libraries are required to compile the project. Depending on which
additional libraries are found however, extra features will be enabled at
compile-time. If you do not want/need these features, you can disable them by
passing the corresponding flag to `cmake` (or by toggling them with `ccmake`):

| Library   | Feature                                                   | Disable with         |
|-----------|-----------------------------------------------------------|----------------------|
| ncurses   | Simple TUI displaying a live summary of the decoded data  | `-DENABLE_TUI=OFF`   |
| portaudio | Support for reading samples live from an audio device     | `-DENABLE_AUDIO=OFF` |


To compile and install:
```
mkdir build && cd build
cmake ..
make
sudo make install
```

Usage
-----
```
sondedump [options] file_in
   -a, --audio-device <id> Use PortAudio device <id> as input (default: choose interactively)
   -c, --csv <file>             Output data to <file> in CSV format
   -f, --fmt <format>           Format output lines as <format>
   -g, --gpx <file>             Output GPX track to <file>
   -k, --kml <file>             Output KML track to <file>
   -l, --live-kml <file>        Output live KML track to <file>
   -r, --location <lat,lon,alt> Set receiver location to <lat, lon, alt> (default: none)
   -t, --type <type>            Enable decoder for the given sonde type. Supported values:
                                auto: Autodetect
                                rs41: Vaisala RS41-SG(P,M)
                                dfm: GRAW DFM06/09
                                m10: MeteoModem M10
                                ims100: Meisei iMS-100

   -h, --help                   Print this help screen
   -v, --version                Print version info

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

TUI keybinds:
   Arrow keys: change active decoder
   Tab: toggle between absolute (lat, lon, alt) and relative (az, el, range) coordinates (requires -r, --location)
```

Examples:
- Use Portaudio device 1 and output to CSV: `sondedump --audio-device 1 --csv
  data.csv`
- Read from file and generate GPX track: `sondedump --gpx track.gpx
  <recording.wav>`
