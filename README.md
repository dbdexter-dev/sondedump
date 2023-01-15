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

| Manufacturer | Model       | GPS                | Temperature        | Humidity           | XDATA              |
|--------------|-------------|--------------------|--------------------|--------------------|--------------------|
| Vaisala      | RS41-SG     | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark: |
| Meteomodem   | M10         | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark: |                    |
| Meteomodem   | M20         | :heavy_check_mark: | :heavy_check_mark: |                    |                    |
| GRAW         | DFM06/09/17 | :heavy_check_mark: | :heavy_check_mark: |                    |                    |
| Meisei       | iMS-100     | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark: |                    |
| Meisei       | RS-11G      | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark: |                    |
| InterMet     | iMet-1/4    | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark: |
| Meteolabor   | SRS-C50     | :heavy_check_mark: | :heavy_check_mark: |                    |                    |


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
`sondedump -h` to see all the available options.

Examples:
- Use Portaudio device 1 and output to CSV: `sondedump --audio-device 1 --csv
  data.csv`
- Read from file and generate GPX track: `sondedump --gpx track.gpx
  <recording.wav>`
- Initialize TUI using Portaudio device 0, and start decoding a RS41 sonde:
  `sondedump -a 0 -t rs41 -T`
