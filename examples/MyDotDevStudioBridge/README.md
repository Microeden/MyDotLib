# MyDot Dev Studio Bridge

This example is a serial protocol bridge for the Microeden Dev Studio
web editor. It accepts one newline-terminated command at a time, exposes the
MyDot peripheral, Wi-Fi, cloud, SD, display, sensor, button and widget APIs,
generic I²C bus operations for sensor blocks, and can execute a stored sequence
without blocking the main loop.

## Quick start

1. Insert a FAT-formatted microSD card when you want persistent sequences.
2. Upload the sketch to a supported MyDot board.
3. Open a serial terminal at **115200 baud** and send `HELP` or `CAPS`.
4. Build a sequence with `CLEAR`, repeated `ADD ...` commands, then `SAVE`.
5. After a reboot, an existing runtime file is loaded and started automatically
   so the project resumes after a power interruption. `LOAD` remains available
   to reload it manually and `RUN` can restart it after `STOP`.

The file is always named `/MyDot.run`. It contains a small header,
an XTEA-CTR encrypted command payload and a CRC32 integrity check. The key is
compiled into the firmware: this protects the card from casual inspection,
but is not a replacement for secure key storage if firmware extraction is in
scope. Do not put long-lived cloud credentials in a sequence unless the serial
transport and the board are trusted.

## Protocol examples

```text
CAPS
CLEAR
ADD BRIGHTNESS 64
ADD PIXELS 0 80 255
ADD DELAY 1000
ADD CLEAR_PIXELS
SAVE
RUN
```

Control flow is available inside saved sequences. Labels are case-insensitive;
`REPEAT` blocks are bounded and may be nested up to eight levels:

```text
CLEAR
ADD LABEL WATERING
ADD WAIT_BUTTON A CLICKED 60000
ADD RELAY ON
ADD DELAY 1000
ADD RELAY OFF
ADD GOTO WATERING
SAVE
RUN
```

Use `IF_BUTTON A|B PRESSED|CLICKED GOTO label` for conditional branches,
`BREAK` or `CONTINUE` inside a `REPEAT <count> ... ENDREPEAT` block, and
`WAIT_BUTTON A|B PRESSED|CLICKED [timeoutMs]` to pause without blocking the
serial, Wi-Fi or cloud services. Button reads through `BUTTON A/B PRESSED` and
`BUTTON A/B CLICKED` remain available for one-shot diagnostics.

Runtime variables are named values held by the interpreter. They are written
with `SET`, read with `GET`/`VARS`, and referenced as `$NAME` (or `${NAME}`) in
later commands. `DIGITAL_READ` and `ANALOG_READ` store their result in a
variable, so sensor values can be reused by display, serial, brightness or
`IF_VAR` commands:

```text
ADD ANALOG_READ A0 SOIL
ADD SERIAL soil=$SOIL
ADD IF_VAR SOIL > 2200 GOTO DRY
ADD BRIGHTNESS 32
ADD GOTO END
ADD LABEL DRY
ADD BRIGHTNESS 255
ADD LABEL END
```

`RELAY STATUS <variable>` reads the current relay state without changing it and
stores `ON` or `OFF` in the given runtime variable. The value can be consumed
by the next block or by a conditional block.

Variables live in RAM while the bridge is running. A `SET` command included in
the saved sequence recreates its value after `LOAD`/`RUN`; the current live
values are not written as a separate persistent state file.

Use `EXEC <command>` for an immediate operation. `RUN` is non-blocking:
`DELAY` yields to the serial parser and to `MyDot::run()`, so Wi-Fi and MQTT
reconnection remain active while a sequence is running. `STOP` halts the
sequence and switches off the relay, fan and pixels.

The Dev Studio also has a priority USB control lane: it sends the single byte
ETX (`0x03`) for an emergency stop. The bridge checks this byte while reading
or writing the microSD and while the runtime is active, so STOP does not wait
behind a long layout transfer. Older firmware continues to work through the
normal text `STOP` command, but it must be updated to this example to get the
priority behavior.

The complete command grammar and the list of supported API operations are in
the introductory comment of `MyDotDevStudioBridge.ino`; the same information
is available at runtime through `HELP` and the machine-readable `CAPS` command.

See [`COMMANDS.md`](COMMANDS.md) for the complete serial command reference.
La versione italiana è disponibile in
[`COMMANDS_IT.md`](COMMANDS_IT.md).

For relay, NeoPixel and fan outputs, power the MyDot carrier from its external
DC jack. USB power alone may not provide enough current for the output stages.
