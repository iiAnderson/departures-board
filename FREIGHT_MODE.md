# Freight mode (fork)

This fork adds a fourth board mode, **Freight**, alongside Rail, Tube and Bus. It shows
freight, light engine and other non-passenger trains at any timing point (TIPLOC), including
places with no station such as Ashton Junction, Bristol (`ASHJN`).

The data comes from a **freight-tracker** server on your network (`darwin-connect/freight-tracker`),
which follows Network Rail's live train movements and timetable. The board fetches
`http://<server>:8090/board/<TIPLOC>.json` every 30 seconds.

```
 Ashton Junction                                   15:42:10
 10:57 6C92  Portbury Auto Terminal (GBRf)        Exp 10:58
 17:20 6B92  Margam TC GBRF (GBRf)               Not active
 6C92 calling, from Margam TC GBRF, Parson Street 10:53 (1 late)
```

- **Left:** booked time at the location and the headcode. A leading `+` means tomorrow.
- **Middle:** destination and operator.
- **Right:** `Not active` (not activated today), `Activated` (waiting to leave), `On time` /
  `Exp hh:mm` (running, estimated from its last report), `Passed`, or `Cancelled`.
- **Third line:** cycles through the remaining trains, a detail line for each of the two trains
  shown (origin and last report), and the data attribution.

## Setup

1. Make sure freight-tracker is running and watching your TIPLOC. In a browser, open
   `http://192.168.68.137:8090/board/ASHJN.json`; you should get JSON back.
2. Install this firmware (see below).
3. In the board's web page choose **Freight**, then enter:
   - **freight-tracker server:** `http://192.168.68.137:8090`
   - **TIPLOC:** `ASHJN`
   - press **Test**, which checks the server and fills in the display name, then **Save**.

Freight works with the scheduler, the carousel and the touch sensor's mode switching, like the
other modes.

## Before installing: turn off automatic updates

In the web page, **Options** tab, untick **Automatic firmware updates at startup** and
**Daily check for firmware updates**.

This build checks *this fork's* GitHub releases rather than gadec-uk's, so it can't be silently
replaced by a stock release without freight mode. The fork has no releases, so with the option on,
the board would show "Firmware Update Check Failed" for 15 seconds on every boot.

## Deploying to the board (over Wi-Fi, no USB needed)

Build (or use the prebuilt file):

```bash
pio run -e esp32dev
# output: .pio/build/esp32dev/firmware.bin
```

Then:

1. **Back up your settings (optional but recommended).** Open `http://<board-ip>/dir` and
   save `config.json` and `apikeys.json` (click to view, then copy). Firmware updates don't
   touch these files, but it's a cheap insurance policy.
2. Open `http://<board-ip>/update`, choose `firmware.bin` (**only** `firmware.bin`; the
   other .bin files are for USB flashing), and upload.
3. The board restarts. The firmware version at boot ends in **F** (e.g. `B3.10-W3.6F`).

How it works: the ESP32 has two app slots. `/update` writes the new firmware into the slot
that isn't running and boots it, so **the firmware you were running before stays intact in the
other slot**. Settings live in a separate filesystem partition and are kept.

## Going back to the original firmware

From easiest to most drastic:

1. **`/rollback` (instant, no download).** Open `http://<board-ip>/rollback` to see what's in
   the other slot, then `http://<board-ip>/rollback?confirm=1` to boot it. Before switching, it
   resets the board mode to Rail/Tube/Bus and removes freight entries from the scheduler and
   carousel, because stock firmware doesn't know freight mode and would otherwise sit on the
   startup screen.
   - Right after your first upload, the other slot holds the stock firmware you had before.
   - After a second freight upload, both slots hold freight builds, so use option 2.
2. **Upload the official firmware.** Download `firmware.bin` from
   <https://github.com/gadec-uk/departures-board/releases> and upload it at `/update`.
   If the board was left in Freight mode, the stock web page shows no mode selected: pick
   Rail, Tube or Bus and save. Then turn automatic updates back on if you want them.
3. **USB reflash.** Use the web installer at <https://departures-board.github.io> (Chrome or
   Edge), or `esptool` with the four .bin files from a release (see the main README). Settings
   usually survive, but keep the backup from step 1 of deploying.
4. **Factory reset** (`/factoryreset`) wipes settings, API keys and Wi-Fi, if all else fails.

## Code changes

- `lib/freightDataClient/`: HTTP + ArduinoJson client for the freight-tracker board JSON.
- `src/Departures Board.cpp`: `MODE_FREIGHT` (3), which reuses the bus board layout and loop;
  settings `freightUrl`, `freightTiploc`, `freightName`; `/rollback` endpoint;
  `FIRMWARE_VARIANT` suffix on the version.
- `web/index.htm`: Freight mode button, settings and Test button.
- `platformio.ini`: update check points at `iiAnderson/departures-board`, version suffix `F`.
