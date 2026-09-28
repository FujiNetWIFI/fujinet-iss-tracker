# ISS Tracker for Amiga + FujiNet NIO

Shows the International Space Station on a 32-colour world map. It runs in a
window on its own 320×256 (PAL) or 320×200 (NTSC) lores screen. The map
shows:

- the ISS as a colour-cycling hardware sprite;
- an astronaut who goes on a spacewalk around the station every two
  minutes (a second hardware sprite);
- day/night shading for the current time, with a soft terminator;
- city lights on the night side, placed from NASA's Black Marble satellite
  imagery of the Earth at night;
- the ISS ground track from roughly the last four hours (two to three orbits);
- latitude, longitude and UTC time in a panel below.

Press **W** to open a window listing who is in space. The ISS position comes
from [Open Notify](http://open-notify.org/)
(`http://api.open-notify.org/iss-now.json`) and refreshes every 60 seconds.
The crew list comes from `astros.json`.

This client uses the [fujinet-nio](https://github.com/markjfisher/fujinet-nio)
stack instead of fujinet-lib. It links
[fujinet-nio-lib](https://github.com/markjfisher/fujinet-nio-lib), which
talks to the resident `fujinet-nio.device`. FujiNet-side JSON queries aren't
available on that path, so the small JSON responses are parsed on the
Amiga.

It runs on any Amiga with Kickstart/Workbench 1.3 or later and 512K. It
only uses V33 OS calls, the nix13 C runtime and no floating point.

## Building

Requires:

- [amiga-gcc](https://github.com/bebbo/amiga-gcc) (`m68k-amigaos-gcc`) on
  `PATH`;
- a checkout of `fujinet-nio-lib`, with `fujinet-nio-driver` next to it;
- `xdftool` from [amitools](https://github.com/cnvogelg/amitools) (or `uvx`)
  for the disk image.

```sh
export FUJINET_NIO_LIB=/path/to/fujinet-nio-lib
make            # -> build/ISSTracker
make disk       # -> dist/ISSTracker.adf
make test       # host-side tests of the JSON/geo/terminator logic
```

If `NIO_WORKSPACE` is set (fujinet-nio workspace), `FUJINET_NIO_LIB`
defaults to `$NIO_WORKSPACE/repos/fujinet-nio-lib`. Set `FUJINET_NIO_DRIVER`
if the driver repository is somewhere else.

This port builds standalone. The shared `../makefiles/` tree drives
cc65-family toolchains and doesn't apply to amiga-gcc.

`src/map_data.c` and `icons/*.info` are generated and committed, so a normal
build doesn't need Python:

- `make regen-map` rebuilds the map data from `gfx/map.png` and the
  city light intensities in `gfx/lights.png`. It needs Python 3 and Pillow,
  and writes `build/map-preview.png`.
- `make regen-icons` rebuilds the icons from the `gfx/*.icon.txt` pixel art
  and writes `build/icon-preview.png`.

To rebuild the two PNGs from the full-size NASA images
(`land_shallow_topo_2048.jpg` and `BlackMarble_2016_01deg.jpg` from NASA
Visible Earth / Earth Observatory):

```sh
python3 tools/png2planar.py --source land_shallow_topo_2048.jpg \
    --lights-source BlackMarble_2016_01deg.jpg \
    gfx/map.png gfx/lights.png src/map_data.c
```

## Running

`dist/ISSTracker.adf` isn't bootable. It holds the program, its icons and a
`ReadMe`.

1. Boot Workbench with the FujiNet NIO drivers installed and
   `fujinet-nio.device` loaded. On Workbench 1.3, use `Install-FujiNet-WB13`
   from the NIO release disk and the `fujinet-load-resident` line it prints.
2. Insert the disk, open it and double-click **ISSTracker**. You can also
   run `ISSTracker` from a Shell.

| Key | Action |
|---|---|
| R | Refresh now |
| W | Who's in space |
| S | Send the astronaut on a spacewalk now |
| T | Ground track on/off |
| N | Night shading on/off |
| Q / Esc | Quit (or use the close gadget) |

The same actions are in the Project menu (right mouse button), with Amiga-key
shortcuts.

If `fujinet-nio.device` isn't loaded, the app says so once and keeps
retrying every 15 seconds. A failed fetch keeps the last position on screen
and retries after 5 seconds.

## How it works

The display uses these colour registers:

| Registers | Use |
|---|---|
| Bitplanes 0–3 | The map. |
| Bitplane 4 | The night mask. Colour *n* + 16 is the dark twin of map colour *n*. |
| 4 | Never drawn by day. Its twin, 20, is the city light colour, so a light is pen 4 on the night side. |
| 1–3, 5–7 | Reserved for UI pens. Their twins are the mouse pointer (17–19) and the ISS sprite pair (21–23), which the astronaut shares. |
| 0, 8–15 | The nine map colours that remain. |

The map is composed in an off-screen chip RAM bitmap: terrain, then the
night mask, then the trail. It is then blitted into the window, so menus and
the crew window are never drawn over.

The terminator comes from the solar declination and equation of time, using
integer Q14 trigonometry. For each column, a binary search finds the row
where the sun sets.

## Files

| File | Purpose |
|---|---|
| `src/main.c` | Startup, timer, event loop, menus and keys |
| `src/screen.c` | Screen, window, map compositor, status panel |
| `src/sprite.c` | ISS and astronaut hardware sprites, colour cycling, spacewalk path |
| `src/night.c` | Night mask bitplane |
| `src/trail.c` | Ground track ring buffer |
| `src/who.c` | "Who's in space" window |
| `src/fetch.c` | HTTP GET through fujinet-nio-lib, ISS response parsing |
| `src/json.c` | Minimal JSON value and array extraction |
| `src/geo.c` | Coordinates, UTC, fixed-point trig, terminator |
| `src/map_data.c` | Generated planar map and 32-colour palette |
| `tools/png2planar.py` | Map and night-light images to palette and planar data |
| `tools/mkinfo.py` | Pixel art to Workbench 1.3 `.info` icons |
| `tests/test_logic.c` | Host tests for the portable logic |

## Credits

- The world map is derived from NASA Visible Earth's
  [Blue Marble](https://visibleearth.nasa.gov/images/57752/blue-marble-land-surface-shallow-water-and-shaded-topography),
  which is public domain.
- City lights come from NASA Earth Observatory's
  [Black Marble 2016](https://earthobservatory.nasa.gov/features/NightLights),
  which is public domain.
- ISS position and crew data come from [Open Notify](http://open-notify.org/)
  by Nathan Bergey.
