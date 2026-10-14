# Joule Observer WiFi dashboard

The ESP32 makes its own WiFi network. Join it with a phone and a live dashboard opens by itself. It shows V RMS, I RMS and power, with the waveform below and a Voltage/Current toggle.

| Path | What it is |
|---|---|
| `src/main.cpp` | ESP32 firmware: WiFi network, captive portal, reads the ATmega on UART2, mock mode |
| `web/index.html` | The dashboard page. The build puts it inside the firmware. |
| `docs/spec.md` | The UART lines the ATmega sends, and the JSON the page reads |
| `platformio.ini` | Board (`esp32dev`) and the two builds, `esp32dev` and `mock` |

## Try the page with no hardware

Double-click `web/index.html` in File Explorer (in VS Code, right-click it and choose Reveal in File Explorer). It opens in your browser with made-up data, and the status line says "Demo data". To work on the page, edit the file and refresh.

Nothing runs a web server on your PC, so there's no `localhost` address. The browser reads the file directly, and the address bar starts with `file:///C:/`. If you open the page through a VS Code preview or Live Server instead, add `?demo` to the end of its address. Otherwise it looks for an ESP32 and says it can't reach one.

## Flash the ESP32

1. In VS Code, open the `Application` folder (File → Open Folder) so PlatformIO finds `platformio.ini`.
2. Plug the DevKitC in over USB. A new port called "Silicon Labs CP210x USB to UART Bridge (COM*n*)" should appear in Device Manager under Ports (COM & LPT). If it doesn't, try another cable (some only charge) or install Silicon Labs' CP210x driver.
3. Upload one of the two builds. In the PlatformIO sidebar, use Project Tasks → *build name* → Upload, or run the command in a PlatformIO terminal:

| Build | Command | Use it when |
|---|---|---|
| `mock` | `pio run -e mock -t upload` | No ATmega yet. The ESP32 makes up readings. |
| `esp32dev` | `pio run -e esp32dev -t upload` | The ATmega is wired to GPIO16/17 and sends the `$M`/`$W` lines in `docs/spec.md` |

`pio device monitor` shows the ESP32's USB log (115200 baud), including every line it receives from the ATmega or the mock.

The page lives inside the firmware, so there's no separate step to upload it (no "Upload Filesystem Image"). Upload again after you edit `web/index.html`.

If the upload stops at `Connecting....` and fails, hold the BOOT button on the board while it connects. Close the serial monitor before uploading, because it holds the COM port.

Libraries: WiFi, WebServer and DNSServer come with the ESP32 Arduino core, so there's nothing extra to install. The page is compiled in, so there's no LittleFS upload either.

## Open the dashboard on a phone

1. Power the ESP32.
2. On the phone, join the WiFi network **JouleObserver**. It has no password.
3. The phone opens its "sign in to network" screen, showing the dashboard. On Android, tap the "Sign in to Wi-Fi network" notification if it doesn't open by itself.
4. If nothing opens, go to http://192.168.4.1 in the browser. On Android, turn mobile data off first, or the phone may try to load it over 4G/5G.

On iPhone the dashboard opens in a sign-in sheet. Keep it open while you watch, because closing it can make iOS leave the network.

A laptop works too. Join JouleObserver from the Windows WiFi menu, then open http://192.168.4.1. The laptop has no internet while it's connected.

The status line under the title tells you what you're looking at:

| Status | Meaning |
|---|---|
| Live (green dot) | Real readings from the ATmega |
| Simulated data from the mock firmware | The `mock` build is running |
| Waiting for the measurement board | The ESP32 works, but no valid line has arrived on UART2. Check `pio device monitor`. No lines at all points to wiring, the level shifter, 9600 baud or a missing ground. Lines that show up there but don't reach the dashboard don't match `docs/spec.md`. |
| No new readings for N s | The ATmega has stopped sending |
| Can't reach the ESP32 | The phone has left the JouleObserver network |
