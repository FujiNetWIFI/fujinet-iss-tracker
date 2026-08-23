# ISS Tracker for Intellivision + FujiNet

Displays a world map with a color-cycling satellite MOB at the current
position of the International Space Station, with latitude, longitude and a
UTC timestamp in the black area below the map. The position is fetched from
[Open Notify](http://open-notify.org/) (`http://api.open-notify.org/iss-now.json`)
through the FujiNet network device every 60 seconds, using FujiNet-side JSON
parsing — the same flow as the other clients in this repository.

Written in IntyBASIC. The JSON channel-mode support (`net_chanmode` /
`net_parse` / `net_query`) is an addition to the standard Intellivision
FujiNet mailbox library carried in `fujinet.bas`; the mailbox bridge forwards
the commands verbatim and fujinet-firmware's network device already
implements them, so no firmware changes are required.

## Building

Requires [IntyBASIC](https://github.com/nanochess/IntyBASIC) (v1.4.2+) and
`as1600` from the jzIntv SDK:

```sh
make            # -> iss.bin (+ iss.cfg) and iss.rom
```

Override tool locations if they are not on PATH:

```sh
make INTYBASIC=/path/to/intybasic LIBDIR=/path/to/IntyBASIC/intybasic/
```

This port builds standalone — the shared `../makefiles/` tree drives
cc65-family toolchains and does not apply to IntyBASIC.

`map.bas` is generated from `iss-intv.png` and committed; regenerate it with
`make regen-map` (needs Python 3 + Pillow) after changing the image. See
`workfiles/README.md` for how the map pipeline fits the 64-card GRAM budget.

## Running

`./run.sh` (or `make run`) launches `iss.rom` in the FujiNet-patched jzIntv
against a fujinet-firmware instance over BoIP (default `localhost:9995`;
override with `FUJINET_TARGET=host:port`). On real hardware, copy `iss.rom`
onto the cartridge SD, or serve it from a TNFS host and boot it through
CONFIG.

The bottom rows show `CONNECTING...` until the first fetch lands, or
`NO FUJINET MAILBOX` if no FujiNet is present. A failed fetch keeps the
last known position on screen and retries after ~5 seconds; successful
cycles refresh once a minute.

## Files

- `iss.bas` — the app: map/MOB display, lat/lon parsing and table lookup,
  epoch-to-UTC conversion (decimal-string long division — the epoch does
  not fit in 16 bits), and the fetch/refresh loop.
- `fujinet.bas` — the reusable FujiNet mailbox library, plus the JSON
  channel-mode additions.
- `map.bas` — generated map data: GRAM cards, BACKTAB layout, the
  longitude/latitude → pixel lookup tables and the satellite bitmap.
- `constants.bas` — standard IntyBASIC constants library.
- `workfiles/` — the PNG → `map.bas` generator and its documentation.

## Credits

- World map derived from
  [Equirectangular projection SW.jpg](https://commons.wikimedia.org/wiki/File:Equirectangular_projection_SW.jpg)
  by Daniel R. Strebe, used under
  [CC BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/deed.en).
- ISS position data from [Open Notify](http://open-notify.org/) by Nathan
  Bergey.
- Satellite bitmap shared with the other FujiNet ISS tracker clients.
