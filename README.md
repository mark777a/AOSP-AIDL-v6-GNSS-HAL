# android.hardware.gnss-service.rpi

An AIDL v6 GNSS HAL implementation for Android Automotive OS (AOSP 16 / "Raspberry Vanilla") on Raspberry Pi 5 8GB, backed by a plain USB NMEA 0183 GPS receiver.

Instead of talking to a vendor GNSS chip binary blob, this HAL reads standard NMEA text sentences off a USB-serial GPS dongle and translates them into the Android `IGnss` AIDL interface (`GnssLocation`, `GnssSvInfo`, raw NMEA callbacks).

## Tested hardware

| Receiver | Interface | Typical device node | Notes |
|---|---|---|---|
| **u-blox 7** USB GPS dongle | USB CDC-ACM | `/dev/ttyACM0` | Default output is usually 9600 baud, GGA/GSA/GSV/RMC enabled by default. |
| **VK-162 G-Mouse** USB GPS dongle | USB CDC-ACM (CH340/PL2303-based on some batches) | `/dev/ttyACM0` or `/dev/ttyUSB0` depending on the onboard USB-serial chip | Also 9600 baud by default; some clones enumerate as `ttyUSB*` instead of `ttyACM*`, which is why the HAL falls back to both. |

If you use a different receiver, confirm its actual output baud rate (some ship configured for 38400 or 115200) and update `kSerialBaudRate` in `NmeaReader.cpp` accordingly.

## How it works

```
USB GPS dongle (NMEA text @ 9600 baud)
        │
        ▼
 NmeaReader (background thread)
   - opens /dev/ttyACM* or /dev/ttyUSB*
   - configures the tty (raw mode, baud rate)
   - poll()s the fd and read()s raw bytes
        │
        ▼
 NmeaParser
   - buffers bytes until full "$...\r\n" sentences appear
   - verifies the *XX checksum
   - parses RMC / GGA / GSA / GSV into GnssLocation / GnssSvInfo
        │
        ▼
 Gnss (IGnss AIDL implementation)
   - forwards parsed fixes, SV status, and raw NMEA text
     to the Android framework via IGnssCallback
        │
        ▼
 android.hardware.gnss-service.rpi (main.cpp)
   - registers the Gnss instance with servicemanager
   - joins the binder thread pool
```

### Sentence handling

| Sentence | Talker IDs accepted | What it contributes |
|---|---|---|
| **RMC** (Recommended Minimum) | `$GPRMC`, `$GNRMC` | Lat/long, UTC date+time, speed over ground, bearing. Only sentences with status `A` (active/valid) are used; `V` (void) fixes are dropped. |
| **GGA** (Fix Data) | `$GPGGA`, `$GNGGA` | Lat/long, altitude. Ignored if the fix-quality field is `0` (no fix). |
| **GSA** (DOP / active satellites) | `$GPGSA`, `$GNGSA` | HDOP, used to derive `horizontalAccuracyMeters` / `verticalAccuracyMeters`. Does not emit a location update by itself. |
| **GSV** (Satellites in View) | `$GPGSV`, `$GLGSV`, `$GAGSV` | Per-satellite elevation/azimuth/CN0, assembled across the multi-sentence GSV batch and reported once the last message in the sequence arrives. |

Every sentence is checksum-verified before use; anything that fails is logged and dropped rather than parsed.

## Example NMEA 0183 v2.3 sentences

These illustrate the format the parser expects (values are representative, not from a live fix):

```
$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A
$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47
$GPGSA,A,3,04,05,,09,12,,,24,,,,,2.5,1.3,2.1*39
$GPGSV,3,1,11,03,03,111,00,04,15,270,00,06,01,010,00,13,06,292,00*74
$GPGSV,3,2,11,14,25,170,00,16,57,208,39,18,67,296,40,19,40,246,00*74
$GPGSV,3,3,11,22,42,067,42,24,14,311,43,27,05,244,00*4D
```

Field breakdown for `$GPRMC`:

```
$GPRMC, 123519,   A,     4807.038,N, 01131.000,E, 022.4, 084.4, 230394, 003.1,W  *6A
        |         |      |          |            |       |      |       |
        UTC time  status latitude   longitude    speed   bearing UTC    magnetic
        hhmmss    A/V    ddmm.mmm   dddmm.mmm    (knots) (deg)   date   variation
```

`ddmm.mmm` / `dddmm.mmm` (degrees + decimal minutes) is why `parseCoordinate()` always splits two digits before the decimal point off as minutes, regardless of whether the field has 2-digit (latitude) or 3-digit (longitude) degrees.

## Project layout

```
/vendor/brcm/interfaces/gnss/aidl/
├── Android.bp                                    - build rules for the cc_binary
├── android.hardware.gnss-service.rpi.rc          - init service definition
├── android.hardware.gnss-service.rpi.xml         - VINTF manifest fragment (AIDL v6)
├── main.cpp                                      - process entry point, registers the HAL with servicemanager
├── Gnss.h / Gnss.cpp                             - IGnss AIDL implementation, extension stubs
├── NmeaReader.h / NmeaReader.cpp                 - serial I/O thread: device discovery, tty config, read loop
└── NmeaParser.h / NmeaParser.cpp                 - sentence framing, checksum, NMEA -> GnssLocation/GnssSvInfo
```

Device-tree files that live *outside* this directory but are required for the HAL to work correctly on real hardware:

```
/device/brcm/rpi5/device.mk                       - Adds android.hardware.gnss-service.rpi to PRODUCT_PACKAGES and copies android.hardware.location.gps.xml so PackageManager.FEATURE_LOCATION_GPS reports as supported.
/device/brcm/rpi5/ramdisk/ueventd.rpi5.rc         - Hotplug-safe /dev/ttyACM*, /dev/ttyUSB* permissions
/device/brcm/rpi5/sepolicy/file_contexts          - Vendor sepolicy allowing the gnss-rpi domain to open the tty and use binder
/device/brcm/rpi5/sepolicy/hal_gnss_rpi.te        - Declares the hal_gnss_rpi domain and ties it into the hal_gnss HAL attribute (via hal_server_domain), granting rw + ioctl access to gps_device and binder access to servicemanager.
/device/brcm/rpi5/sepolicy/device.te              - Declares gps_device as a dev_type. This is the conventional AOSP name for exactly this purpose
```

## Building

Everything required is already wired up across the files listed above — this section is what to double-check, not what to newly do:

- **Binary build & packaging**: `Android.bp` builds `android.hardware.gnss-service.rpi` and installs both `android.hardware.gnss-service.rpi.rc` (via `init_rc`) and the VINTF fragment (via `vintf_fragments`) automatically — nothing extra needed for these two.
- **Getting into the image at all**: `device.mk`'s `PRODUCT_PACKAGES += android.hardware.gnss-service.rpi` is what actually causes the binary to be built and installed to `/vendor/bin/hw/`. Without this line, `Android.bp` being correct doesn't matter — the module is never pulled into the build.
- **Device node permissions**: `ueventd.rpi5.rc` handles Unix ownership/mode for `/dev/ttyACM*`/`/dev/ttyUSB*` dynamically as they enumerate — this is why permissions aren't set in the service `.rc` file (a boot-time trigger there would miss devices that show up after boot).
- **SELinux**: `hal_gnss_rpi.te`, `device.te`, and `file_contexts` together label the device node, label the executable, and grant the domain the access it needs (including `ioctl`, required for the termios calls in `NmeaReader::configureSerialPort()`).


## Runtime behavior / reconnection

- On start, `NmeaReader` globs for `/dev/ttyACM*` first, falling back to `/dev/ttyUSB0` if none is found.
- If no device is present, it retries every 15s.
- The tty is opened `O_RDWR | O_NOCTTY | O_NONBLOCK` and put into raw mode at a fixed baud rate before any data is read.
- If the device disappears mid-stream (unplugged, USB reset, etc.), the fd is closed and reopened automatically after a short backoff, without restarting the service.

## Verifying it's working

```
# Confirm the HAL registered correctly
adb shell lshal | grep gnss

# Watch HAL logs
adb logcat | grep -i gnss

# Check the framework is receiving fixes
adb shell dumpsys location
```

## Known limitations

- Only `IGnss` core functionality (location, NMEA text, SV status) is implemented; all extension interfaces (`AGnss`, `GnssMeasurement`, `GnssBatching`, `GnssGeofence`, PSDS, measurement corrections, etc.) return `EX_UNSUPPORTED_OPERATION`. This is standards-compliant (all extensions are optional) but means no assisted-GNSS, no raw measurements, and no batching.
- `horizontalAccuracyMeters` / `verticalAccuracyMeters` are a heuristic estimate derived from GSA's HDOP (`2.5 * HDOP`), not a receiver-reported accuracy — treat it as approximate.
- `injectTime` / `injectLocation` / `injectBestLocation` / `deleteAidingData` are accepted but are no-ops; there's no aiding-data store for this receiver class.
- Only one fix source is supported at a time (no multi-constellation fusion beyond what a single combined-talker "GN" sentence already provides).

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| `lshal` shows the HAL registered but no fixes ever arrive | Wrong baud rate for your receiver, or the receiver hasn't acquired satellites yet (check with a known-good tool like `u-center` or `gpsmon` first). |
| Logcat shows repeated "GNSS device not found" | Device node permissions — confirm `ueventd.rc` has the `ttyACM*`/`ttyUSB*` rules and that `ls -l /dev/ttyACM0` shows `gps:system`. |
| Logcat shows repeated checksum failures | Baud rate mismatch (garbled bytes) or a bad/loose USB cable. |
| HAL picks the wrong device on a Pi with multiple USB-serial peripherals plugged in | `findDevice()` takes the first `ttyACM*` match; unplug the other device, or adjust `findDevice()` to match a specific `/dev/serial/by-id/*` path. |
