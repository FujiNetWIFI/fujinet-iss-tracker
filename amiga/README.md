# ISS Tracker for Amiga + FujiNet NIO

Shows the International Space Station on a 32-colour world map. It runs in a
window on its own 320×256 (PAL) or 320×200 (NTSC) lores screen. The map
shows:

- the ISS as a colour-cycling hardware sprite, moving smoothly between
  fixes (dead-reckoned every second from the last two);
- the ISS's visibility footprint: the circle of places that can see it;
- an astronaut who goes on a spacewalk around the station every two
  minutes (a second hardware sprite);
- day/night shading for the current time, with a soft terminator;
- city lights on the night side, placed from NASA's Black Marble satellite
  imagery of the Earth at night, twinkling town by town;
- the ISS ground track from roughly the last four hours (two to three orbits);
- latitude, longitude and UTC time in a panel below, ticking live, with
  the country or ocean the ISS is over;
- optionally your home: a crosshair on the map, the distance to the ISS,
  and an alert when the ISS comes over your horizon;
- now and then (every 5–15 minutes, or on **U**) a flying saucer that
  swoops in from off the map, hovers while its little green pilot pokes its
  head out for a look around, and shrinks away off another edge.

It plays Paula sound effects: a sonar ping for each fix, three pings when
the ISS comes into view of home, NASA "Quindar" beeps as an astronaut
goes out and comes back in, and a theremin warble while a UFO is about. After 10 idle minutes (or **B**) it switches to
a screen saver with just the map and the ISS.

Press **W** to open a window listing who is in space, and **H** for a
window listing the keys and the current settings. The ISS position comes
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
make test       # host-side tests of the JSON/geo/terminator/UFO path logic
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
- `make regen-regions` rebuilds the "what's below" data from
  `gfx/regions.png` and `gfx/regions.txt`. To rebuild those from Natural
  Earth's GeoJSON, run `tools/mkregions.py --countries
  ne_110m_admin_0_countries.geojson --marine
  ne_110m_geography_marine_polys.geojson gfx/regions.png gfx/regions.txt
  src/region_data.c`.
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
| U | UFO sighting now |
| V | Viewing circle on/off |
| M | Sound on/off |
| B | Screen saver (any key or click returns) |
| H (or ?) | Help: the keys and the current settings |
| T | Ground track on/off |
| N | Night shading on/off |
| Q / Esc | Quit (or use the close gadget) |

The same actions are in the Project menu (right mouse button), with Amiga-key
shortcuts.

If `fujinet-nio.device` isn't loaded, the app says so once and keeps
retrying every 15 seconds. A failed fetch keeps the last position on screen
and retries after 5 seconds.

### Settings

Set these as ToolTypes in the icon (select it, then Info from the Workbench
menu), or as Shell arguments, e.g. `ISSTracker HOMELAT=51.48 HOMELON=-0.01`:

| Setting | Meaning |
|---|---|
| `HOMELAT=`, `HOMELON=` | Your location in decimal degrees (south and west negative). Needs both. |
| `SAVER=` | Idle minutes before the screen saver; `0` turns it off. Default 10. |
| `SOUND=OFF` | Start with sound effects off. |
| `CIRCLE=OFF` | Start with the viewing circle hidden. |

The icon ships with example `(HOMELAT=40.71)` and `(HOMELON=-74.01)` entries
in brackets, which Workbench ignores: fill in your own location and remove
the brackets. The footprint is the horizon circle for the ISS's ~420 km
altitude (20.3 degrees of arc, about 2,250 km), so "in view" means above the
horizon, not necessarily high in the sky.

## How it works

The display uses these colour registers:

| Registers | Use |
|---|---|
| Bitplanes 0–3 | The map. |
| Bitplane 4 | The night mask. Colour *n* + 16 is the dark twin of map colour *n*. |
| 4 | Never part of the map. Its twin, 20, is the city light colour, so a light is pen 4 on the night side; 4 itself is the UFO pilot's green. |
| 1–3, 5–7 | Reserved for UI pens. Their twins are the mouse pointer (17–19) and the ISS sprite pair (21–23), which the astronaut shares. |
| 0, 8–15 | The nine map colours that remain. |

The map is composed in an off-screen chip RAM bitmap: terrain, then the
night mask, then the trail. It is then blitted into the window, so menus and
the crew window are never drawn over.

No hardware sprite is left for the UFO (sprites 4–7 would share night map
colours), so it is a masked blit. Each frame the map under its old and new
positions is copied into a scratch bitmap, the saucer is cut in there, and
the result is blitted into the window in one go, so it never flickers. A
second timer runs at 25 frames a second only while one is flying.

The terminator comes from the solar declination and equation of time, using
integer Q14 trigonometry. For each column, a binary search finds the row
where the sun sets.

## Files

| File | Purpose |
|---|---|
| `src/main.c` | Startup, timer, event loop, menus and keys |
| `src/screen.c` | Screen, window, map compositor, status panel, pop-up windows |
| `src/sprite.c` | ISS and astronaut hardware sprites, colour cycling, spacewalk path |
| `src/night.c` | Night mask bitplane |
| `src/trail.c` | Ground track ring buffer |
| `src/who.c` | "Who's in space" window |
| `src/help.c` | Help window: keys and current settings |
| `src/ufo.c` | UFO sightings: saucer images and flicker-free blitting |
| `src/ufo_path.c` | UFO flight path, timeline and pixel art (portable, host-tested) |
| `src/fetch.c` | HTTP GET through fujinet-nio-lib, ISS response parsing |
| `src/json.c` | Minimal JSON value and array extraction |
| `src/geo.c` | Coordinates, UTC, fixed-point trig and inverse trig, great-circle maths, dead reckoning, terminator |
| `src/home.c` | Home crosshair, visibility footprint, distance and in-view test |
| `src/region.c` | "What's below" lookup |
| `src/sound.c` | Paula sound effects through `audio.device` |
| `src/twinkle.c` | Twinkling city lights |
| `src/config.c` | ToolType / Shell argument settings |
| `src/map_data.c` | Generated planar map and 32-colour palette |
| `src/region_data.c` | Generated 1-degree country/ocean grid |
| `tools/mkregions.py` | Natural Earth outlines to the region grid |
| `tools/png2planar.py` | Map and night-light images to palette and planar data |
| `tools/mkinfo.py` | Pixel art to `.info` icons that suit every Workbench palette |
| `tests/test_logic.c` | Host tests for the portable logic |

## Credits

- The world map is derived from NASA Visible Earth's
  [Blue Marble](https://visibleearth.nasa.gov/images/57752/blue-marble-land-surface-shallow-water-and-shaded-topography),
  which is public domain.
- City lights come from NASA Earth Observatory's
  [Black Marble 2016](https://earthobservatory.nasa.gov/features/NightLights),
  which is public domain.
- Country and ocean names and outlines come from
  [Natural Earth](https://www.naturalearthdata.com/) 1:110m data, which is
  public domain.
- ISS position and crew data come from [Open Notify](http://open-notify.org/)
  by Nathan Bergey.
