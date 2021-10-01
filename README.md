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

Compilation:
```
mkdir build && cd build
cmake ..
make
```

Usage
-----
