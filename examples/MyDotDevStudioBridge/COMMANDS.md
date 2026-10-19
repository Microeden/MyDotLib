# MyDot Dev Studio Bridge — Serial command reference

This document is the complete command reference for
`MyDotDevStudioBridge.ino`.

The bridge reads one UTF-8 line at a time from `Serial` at **115200 baud**.
Every command must end with a newline. Commands and subcommands are
case-insensitive; values such as text, Wi-Fi SSIDs and cloud tokens keep their
original case.
Incoming browser data is handled in bounded batches (at most one complete
command per loop pass), leaving time for the resident runtime and network.

The Serial Monitor should use **Newline** as the line ending.

## Structured capabilities

`CAPS` returns protocol version `2`. In addition to the summary lines, it emits
one machine-readable line for every command:

```text
CAPS COMMAND DISPLAY_TEXT args=text type=string
CAPS COMMAND RELAY args=state type=enum values=ON|OFF|TOGGLE|STATUS result=variable
CAPS COMMAND SENSOR_READ args=field,variable type=bme690 values=TEMPERATURE|PRESSURE|HUMIDITY|GAS
```

The command identifier uses underscores so a web editor can use it as a stable
key. `args` lists positional arguments, `type` identifies the API family, and
optional `values`/`range` fields describe accepted values. The summary
`CAPS RUNTIME` line remains available for clients that only need category names.

## Protocol basics

The bridge prints one of these response families:

```text
OK ...       Command accepted
ERR ...      Command rejected or failed
INFO ...     Informational message
STATUS ...   Runtime, connection and resource status
```

The Bridge emits no startup diagnostics when verbose logs are off. Send `STATUS` to check whether
the microSD card initialized and whether a saved runtime is running. USB
power is enough for serial communication, but the MyDot carrier must be
powered through its external DC jack when using the relay, NeoPixels or fan.

## Top-level commands

These commands manage the in-memory sequence or the bridge itself.

| Command | Description |
| --- | --- |
| `HELP` | Prints a short human-readable command list. |
| `CAPS` | Prints the machine-readable command families, limits and file format. |
| `STATUS` | Prints the board model, USB/runtime state plus Wi‑Fi, My Microeden, microSD capacity and ESP32 heap information when supported. |
| `LOGS ON|OFF|STATUS` | Enables, disables or checks unsolicited Bridge and MyDot library diagnostics. Defaults to `OFF` after every boot. |
| `ADD <runtime-command>` | Appends one command to the current sequence. |
| `LIST` | Lists the sequence with zero-based command indexes. |
| `CLEAR` | Immediately acknowledges deletion of the in-memory sequence, then stops execution and switches relay, fan and NeoPixels to a safe off state. It does not delete the SD file. |
| `SAVE` | Encrypts the current sequence and writes `/MyDot.run` to SD. |
| `LOAD` | Reads, decrypts, verifies and loads `/MyDot.run`. |
| `RUN` | Starts the current sequence at command zero. Execution is non-blocking. |
| `STOP` | Stops execution, disables the runtime watchdog, switches off relay, fan and NeoPixels, and clears the OLED display. |
| `REBOOT` | Safely stops the runtime and reboots the MyDot board using the native reset primitive of the selected Arduino core. |
| `EXEC <runtime-command>` | Executes one immediate command. Control-flow commands require `RUN` and are rejected by `EXEC`. |

The Bridge is quiet by default: startup messages, runtime diagnostics
(`I2C DATA`, `VAR`, `OK WIDGET WRITE`), runtime warnings/errors and MyDot
Wi-Fi/MQTT messages are not sent to USB. `LOGS ON` enables them temporarily
for debugging; `LOGS OFF` turns them off again, and `LOGS STATUS` reports the
current setting. This setting is not saved to `/MyDot.run`. Replies to
top-level requests (including `EXEC` and emergency `STOP`) and explicit
`SERIAL <text>` runtime instructions are sent when the USB reader can keep up.
For a resident program, `SERIAL` sends at most one line every 100 ms across
all tasks. Faster instructions continue running but their serial lines are
skipped; `EXEC SERIAL` and protocol replies are not rate-limited. This keeps
high-frequency programs from flooding the native USB interface.
`STATUS` also includes `runtimeLogs=on|off`. A stopped runtime can be diagnosed with
`STATUS` after reconnecting, or by enabling logs before reproducing the issue.
Descriptions below of normal serial output from runtime commands apply to
`EXEC` or to the resident sequence with `LOGS ON`.

On Nano ESP32, Nano RP2040 Connect and Nano 33 IoT, the Bridge applies a
bounded time budget to serial output. If the browser stops consuming USB data, runtime output
is dropped instead of queuing without limit or holding up the resident
program. Protocol replies have a separate, longer budget; a reply may be
incomplete if the connection is stalled. `STATUS serialDroppedBytes` counts
discarded output bytes since boot, while `serialSkippedLines` counts resident
`SERIAL` lines skipped by the 100 ms limit. This reduces USB output pressure;
it cannot bound a call stuck inside a board's USB core or guarantee recovery
from a USB-core failure, and does
not replace the watchdog or protect other blocking I/O.

The portable watchdog commands are available as runtime instructions and can
also be sent through `EXEC`:

```text
WATCHDOG BEGIN <timeout-ms>
WATCHDOG FEED
WATCHDOG STOP
```

`BEGIN` enables the cooperative watchdog, `FEED` refreshes its deadline and
`STOP` disables it. The Bridge checks the deadline from `MyDot::run()` and
reboots the board when the timeout expires. Add a `WATCHDOG FEED` node to the
continuous flow; no automatic feed is performed by the library, so a stalled
flow can still be detected. `CAPS FEATURE watchdog=cooperative` identifies
this backend.

`STATUS` keeps the original key/value fields and adds `board` (the readable
board name detected from the Arduino core), `serialDroppedBytes`, `wifi`, `wifiRssi`,
`cloud`, `sdCardBytes`, `sdTotalBytes`, `sdUsedBytes`, `sdFreeBytes`,
`ramFreeBytes` and `ramTotalBytes`. `sdCardBytes` is the physical card
capacity (`cardSize()`); `sdTotalBytes` is the usable filesystem capacity.
Resource sizes are reported when the selected board exposes the corresponding
API; otherwise their value is `unknown`.

On ESP32, `STATUS` also reports `taskWatchdog` (`ready` or `unavailable`),
`resetReason`, `heapLargestBytes` and
`heapMinimumBytes`. After a warm reboot it includes `previousStage`,
`previousIndex`, `previousHeapBytes`, `previousLargestBytes` and
`previousMinimumBytes`, captured
before that reboot without writing flash. If the task watchdog fires again,
these fields identify whether the Bridge was in `network`, a serial request
(`serial-status`, `serial-caps`, `serial-exec` or `serial-other`) or a runtime
`command`; use `LIST` to inspect `previousIndex` for a runtime command. A falling largest
free heap suggests retained allocations; a shrinking largest block with nearly
stable free heap suggests fragmentation.

The Dev Studio also has a priority USB control lane for stopping the board:
the ETX byte (`0x03`) interrupts `RUN` even while a long microSD read or write
is in progress. This requires the updated bridge firmware; older firmware can
still receive the text `STOP` command, but it cannot pre-empt an operation
already in progress.

Example:

```text
HELP
CLEAR
ADD BRIGHTNESS 255
ADD PIXELS 255 0 0
LIST
SAVE
RUN
```

`SAVE` stores the complete textual sequence in its current order, including
delays, labels, variable assignments and cloud commands. It does not store the
current live value of a variable unless that value is represented by a `SET`
command in the sequence.

After a reset the file is detected, loaded and started automatically when the
SD card is available and the integrity check succeeds. This makes the saved
runtime resume operation after a power interruption. `LOAD` remains available
to retry or reload the file manually, and `RUN` can restart the sequence after
`STOP`.

## Runtime limits

| Resource | Limit |
| --- | ---: |
| Sequence commands | Limited by payload size |
| Command length | 96 characters |
| Encrypted script payload | 4096 bytes |
| Nested `REPEAT` blocks | 8 |
| Runtime variables | 48 |
| Variable name length | 16 characters |
| Variable value length | 64 characters |

## Variables and value substitution

Variables are held in RAM by the interpreter. Names are normalized to
uppercase and may contain letters, numbers and `_`.

```text
SET <name> <value>
GET <name>
VARS
UNSET <name>
CLEAR_VARS
```

Reference a variable in any later command with `$NAME` or `${NAME}`:

```text
SET LIMIT 2200
ANALOG_READ A0 SOIL
SERIAL soil=$SOIL limit=$LIMIT
BRIGHTNESS $SOIL
```

`SET` stores the complete remaining text after the variable name. Variable
references are expanded when a runtime command is executed, not when it is
added. This permits a value read from a sensor to be reused later in the same
sequence.

To calculate an integer from runtime variables, start the `SET` value with
`=`. Parentheses, decimal or hexadecimal numbers, and the operators `+`, `-`,
`*`, `/` and `%` are supported; variables are expanded before evaluation.
Without `=`, `SET` keeps copying the value as text.

```text
SET KNOBRAW0 42
SET KNOBRAW1 1
SET KNOB_POSITION = $KNOBRAW0 + (256 * $KNOBRAW1)
GET KNOB_POSITION
```

The third command calculates `298`, combining the two little-endian bytes into
an integer. The result can be used by `IF_VAR`, an output command or another
`SET`.

`BASE_CONVERT <value> <fromBase> <toBase> <variable>` converts integers
between bases 2 and 36 and stores the text result in the named variable. The
matching `0b`, `0o` and `0x` prefixes are accepted for bases 2, 8 and 16. For
example:

```text
BASE_CONVERT FF 16 10 DECIMAL
BASE_CONVERT $DECIMAL 10 2 BINARY
```

The first command stores `255` in `DECIMAL`; the second stores `11111111` in
`BINARY`. Negative values are supported.

`VARS` prints:

```text
BEGIN_VARS
VAR SOIL=1840
END_VARS
```

The live values are volatile. To recreate an initial value after `LOAD`/`RUN`,
include a `SET` command in the saved sequence.

## Mathematics

`MATH <operation> <operands...> <variable>` calculates with decimal numbers
and stores the result in a runtime variable. Operands can be numeric literals,
`PI`, `TAU`, `E`, or existing `$VARIABLE` references. Use a dot as the decimal
separator. The destination name is not substituted, so it can be updated in
place. Results are stored with up to eight decimal places; very large or small
results use scientific notation. `EXEC MATH ...` is also supported.

| Operation | Operands before destination | Meaning |
| --- | --- | --- |
| `PI`, `TAU`, `E` | none | Store the constant (τ = 2π). |
| `ADD`, `SUB`, `MUL`, `DIV`, `MOD`, `POW` | `a b` | Arithmetic; `MOD` is floating-point remainder. |
| `MIN`, `MAX`, `AVG`, `HYPOT` | `a b` | Minimum, maximum, average, Euclidean length. |
| `ABS`, `NEG`, `SIGN` | `a` | Absolute value, negation, sign (`-1`, `0`, `1`). |
| `SQRT`, `CBRT`, `EXP` | `a` | Square root, cube root, e to the power `a`. |
| `LN`, `LOG10`, `LOG2` | `a` | Natural, base-10 and base-2 logarithms. |
| `FLOOR`, `CEIL`, `ROUND`, `TRUNC` | `a` | Integer rounding; `ROUND` rounds halves away from zero. |
| `ROUND_TO` | `a decimals` | Round to 0–8 decimal places. |
| `SIN`, `COS`, `TAN` | `angle` | Trigonometry; input in radians. |
| `ASIN`, `ACOS`, `ATAN` | `a` | Inverse trigonometry; result in radians. |
| `ATAN2` | `y x` | Quadrant-aware angle in radians. |
| `SIN_DEG`, `COS_DEG`, `TAN_DEG` | `angle` | Trigonometry; input in degrees. |
| `ASIN_DEG`, `ACOS_DEG`, `ATAN_DEG` | `a` | Inverse trigonometry; result in degrees. |
| `ATAN2_DEG` | `y x` | Quadrant-aware angle in degrees (−180 to 180). |
| `TO_DEG`, `TO_RAD` | `angle` | Convert radians to degrees or degrees to radians. |
| `SINH`, `COSH`, `TANH` | `a` | Hyperbolic functions. |
| `ASINH`, `ACOSH`, `ATANH` | `a` | Inverse hyperbolic functions. |
| `CLAMP` | `value min max` | Limit a value to the inclusive range. |
| `LERP` | `start end fraction` | Linear interpolation (or extrapolation). |
| `NORM3` | `x y z` | Length of a three-component vector. |
| `MAP` | `value inMin inMax outMin outMax` | Linear range mapping; does not clamp. |

For example, calculate a sensor angle and send it to a cloud widget:

```text
MATH PI CIRCLE_PI
MATH SQRT 9 ROOT
MATH HYPOT $AY $AZ YZ_LENGTH
MATH ATAN2_DEG $AX $YZ_LENGTH PITCH_DEG
SERIAL pitch=$PITCH_DEG
WIDGET WRITE LEVEL pitch $PITCH_DEG
```

`$AX`, `$AY`, and `$AZ` represent accelerometer samples already stored in
variables; the appropriate axis formula depends on the sensor's orientation.
Division or `MOD` by zero, square root of a negative number, logarithm of a
non-positive number, invalid inverse-trigonometric inputs, and non-finite
results return `ERR MATH ...`. As with other invalid runtime commands, this
stops the current `RUN` sequence. For noisy normalized sensor values, use
`CLAMP` before `ASIN` or `ACOS`. The supported operations and their argument
counts are also exposed in `CAPS MATH` lines for Dev Studio integration.

## Digital and analog I/O

Pin arguments accept numeric Arduino pin numbers and board labels such as
`A0`, `A1`, `D2` and `D3`. On the Nano ESP32 the `D*` and `A*` aliases follow
the Arduino Nano pinout.

```text
DIGITAL_READ <pin> <variable>
DIGITAL_WRITE <pin> <value>
ANALOG_READ <pin> <variable>
ANALOG_WRITE <pin> <0..255>
```

`DIGITAL_READ` configures the pin as `INPUT` and stores `0` or `1`. Digital
write values may be `HIGH`, `LOW`, `ON`, `OFF`, `TRUE`, `FALSE` or a number.
`ANALOG_READ` stores the raw ADC value. `ANALOG_WRITE` accepts the PWM range
0–255 where supported by the selected board pin.

Example:

```text
EXEC DIGITAL_READ D4 BUTTON_STATE
EXEC GET BUTTON_STATE
EXEC DIGITAL_WRITE D13 HIGH
EXEC ANALOG_READ A0 SOIL
EXEC SERIAL soil=$SOIL
```

## Generic I²C bus blocks

The bridge exposes the Arduino `Wire` bus as byte-oriented runtime commands.
These primitives are deliberately independent of a particular sensor, so a
web editor can build a dedicated sensor block by emitting one `READ_REG` or
`TRANSFER` command. The bus uses the board's default SDA/SCL pins; no GPIO
numbers are hard-coded in the protocol.

```text
I2C BEGIN [frequency]
I2C SCAN
I2C PING <address>
I2C WRITE <address> <byte0> [byte1 ... byte31]
I2C READ <address> <length> [prefix]
I2C TRANSFER <address> <writeCount> <readCount> <writeByte...> [prefix]
I2C READ_REG <address> <register> <length> [prefix]
I2C WRITE_REG <address> <register> <value>
```

Addresses and byte values accept decimal notation or hexadecimal notation such
as `0x76`. Valid 7-bit device addresses are `0x03`–`0x77`; a transaction can
contain up to 32 bytes. `I2C BEGIN` is optional because `MyDot::begin()` starts
the default bus, but it can be used to select a clock from 10 kHz to 1 MHz.
The normal sensor speed is 100 kHz; use 400000 for devices that support fast
mode.

`SCAN` prints every acknowledged address. `PING` tests one address. `WRITE`
sends a raw byte sequence. `READ` requests bytes without a register phase.
`TRANSFER` performs a combined write-then-read transaction with a repeated
start, which is the common pattern for register-based sensors. Its counts make
the command unambiguous for a visual block:

```text
I2C TRANSFER <address> <number-of-bytes-to-write> <number-to-read> <write-bytes...> [prefix]
```

`READ_REG` is a convenience block for the most common case: it writes one
register byte and reads the requested number of response bytes. `WRITE_REG`
writes one register byte followed by one value byte.

When `prefix` is supplied, every received byte is saved as a runtime variable
named `<prefix>0`, `<prefix>1`, and so on. This lets later blocks use the
result, for example `$WHO0`, in a display, cloud value, condition or SD log.
With verbose logs enabled (or via `EXEC`), the bridge also prints the bytes
in hexadecimal using an `I2C DATA` line.

I²C operations are bounded: on the Nano ESP32 the bridge configures a 50 ms
transaction timeout and retries an incomplete read up to two times. If the
device is temporarily disconnected or holds the bus, the bridge reinitializes
`Wire`, reports `WARN I2C`/`ERR I2C`, and continues the runtime instead of
terminating the saved sequence. Variables from the last complete read remain
available, so a later block can decide how to handle a missing sample.

Examples:

```text
EXEC I2C BEGIN 400000
EXEC I2C SCAN
EXEC I2C READ_REG 0x76 0xD0 1 WHO
EXEC SERIAL sensor-id=$WHO0
```

A dedicated sensor block can therefore contain the address, register, byte
order and conversion formula while using this generic bus command as its only
hardware transport. Keep SDA and SCL pulled up according to the sensor board
and never connect an I²C device above the board's logic voltage.

## Control flow

Control-flow commands are executed inside a sequence started with `RUN`.
`EXEC` rejects them because it has no sequence cursor or loop stack.

The Dev Studio can compile disconnected chains as cooperative tasks. Each task
keeps its own program counter, timer, and wait state, so `DELAY` and
`WAIT_BUTTON` pause only the chain that contains them while the bridge services
the others. A connected chain remains one task; the true and false paths of
`IF_VAR` or `IF_BUTTON` stay mutually exclusive. The Dev Studio generates the
internal `TASK BEGIN ONCE|LOOP` and `TASK END` markers; they are not palette
blocks. Saved programs without these markers keep the previous linear behavior.
Tasks may jump to a shared manual flow outside the `TASK` sections. When that
flow returns to the compiler-generated continuous-loop label, its calling
`ONCE` task finishes or its calling `LOOP` task restarts; the destination loop
is already running independently. Jumping into another task's body is invalid.

### Labels and jumps

```text
LABEL <name>
GOTO <name>
IF_BUTTON A|B PRESSED|CLICKED GOTO <name>
IF_VAR <name> <operator> <value> GOTO <name>
```

Labels are case-insensitive. A jump that leaves a repeat block automatically
unwinds the active loop frames.

`IF_BUTTON` and `IF_VAR` are instantaneous tests: when the condition is false,
the runtime continues with the next line. Use `WAIT_BUTTON` when execution
must actually wait for a press. In the Dev Studio graph, a conditional branch
without a continuation is closed automatically so it cannot accidentally fall
through into a label block.

`IF_VAR` supports numeric comparisons `==`, `=`, `!=`, `>`, `>=`, `<` and
`<=`. Text variables support `==`, `=` and `!=`.

Example:

```text
ADD ANALOG_READ A0 SOIL
ADD IF_VAR SOIL > 2200 GOTO DRY
ADD BRIGHTNESS 32
ADD GOTO END
ADD LABEL DRY
ADD BRIGHTNESS 255
ADD LABEL END
```

### Waiting for a button

```text
WAIT_BUTTON A|B PRESSED|CLICKED [timeoutMs]
```

The command pauses the sequence without blocking `serviceSerial()` or
`MyDot::run()`. With no timeout it waits indefinitely. If the timeout expires,
the sequence continues and prints `INFO WAIT_BUTTON timeout`.

The bridge prints `INFO WAIT_BUTTON waiting` when the wait is armed and
`INFO WAIT_BUTTON triggered` when the click is detected. Button edges are
latched by the bridge loop, so a short click is not lost while slower work is
being processed.

### Repeat blocks

```text
REPEAT <positive-count>
  ...commands...
ENDREPEAT
```

`BREAK` exits the innermost repeat block. `CONTINUE` jumps to its
`ENDREPEAT`, decrements the loop counter and starts the next iteration.

Example:

```text
ADD REPEAT 3
ADD PIXELS_RANDOM
ADD DELAY 500
ADD ENDREPEAT
```

## Relay and NeoPixels

```text
RELAY ON
RELAY OFF
RELAY TOGGLE
RELAY STATUS <variable>

BRIGHTNESS <0..255>
PIXELS <red> <green> <blue>
PIXELS_SHOW
PIXELS_RANDOM
PIXEL <index> <red> <green> <blue>
CLEAR_PIXELS
```

`RELAY STATUS` reads the current output without changing it and stores the
text `ON` or `OFF` in the selected runtime variable.

Example:

```text
ADD RELAY STATUS RELAY_STATE
ADD SERIAL relay=$RELAY_STATE
```

`PIXELS` and `PIXEL` immediately call `showPixels()`. `PIXELS_SHOW` is
provided for sequences that explicitly control when the current pixel buffer
is sent to the LEDs. Pixel indexes range from `0` to `NUMPIXELS - 1`.

## Fan and DRV8830

```text
FAN <-100..100>
FAN STOP
FAN STATUS
FAN CLEAR_FAULT
```

Positive and negative values select the direction. `STATUS` prints the last
requested speed and the DRV8830 fault register in hexadecimal. `CLEAR_FAULT`
sends the driver clear command; it does not repair an active hardware fault.

## OLED display

The display commands are safe when no OLED is detected. `DISPLAY <text>` is a
compact form; the structured forms expose the individual display methods.

```text
DISPLAY <text>
DISPLAY PRESENT
DISPLAY LOGO
DISPLAY SENSOR
DISPLAY CLEAR
DISPLAY SHOW
DISPLAY RAW_SHOW
DISPLAY CURSOR <x> <y>
DISPLAY SIZE <1..4>
DISPLAY COLOR <0..65535>
DISPLAY TEXT <text>
DISPLAY TEXT_AT <x> <y> <size> <text>
DISPLAY PRINT <text>
DISPLAY PRINTLN <text>
DISPLAY NEWLINE
```

Write operations return `OK DISPLAY TEXT` (or the matching operation). If the
OLED was not detected, the bridge also prints `WARN DISPLAY unavailable`; the
sequence continues, but power, I2C wiring and address `0x3C` should be checked.

`RAW_SHOW` calls the underlying `Adafruit_SSD1306` object returned by
`getDisplay()`.

## BME690 sensor

```text
SENSORS
SENSOR_READ TEMPERATURE|PRESSURE|HUMIDITY|GAS <variable>
SENSORS_LOG <path>
```

Reads the onboard BME690 and prints temperature, pressure, humidity and gas
resistance to the serial port. The hardware sensor is a BME690, accessed using
the compatible Adafruit BME680 driver API.

`SENSORS_LOG` performs the same reading and appends one CSV row to the supplied
SD path. It creates the file and writes the header on the first call. The CSV
columns are `epoch`, `time`, `temperature_c`, `pressure_hpa`,
`humidity_percent` and `gas_kohm`.

For custom output, use `SENSOR_READ` to put one field in a runtime variable and
then reference it in any later command. This also works with `DISPLAY`,
`SERIAL`, `SET`, cloud writes and SD writes:

```text
EXEC SENSOR_READ TEMPERATURE TEMP
EXEC SENSOR_READ HUMIDITY HUM
EXEC DISPLAY T=$TEMP C  H=$HUM %
EXEC SD APPEND_LINE /custom.csv $TEMP,$HUM
```

Example of a one-minute logger:

```text
CLEAR
ADD LABEL LOG
ADD SENSORS_LOG /bme690.csv
ADD DELAY 60000
ADD GOTO LOG
SAVE
RUN
```

Read the resulting file with:

```text
EXEC SD READ /bme690.csv
```

## Buttons

```text
BUTTON STATUS
BUTTON A PRESSED
BUTTON A CLICKED
BUTTON B PRESSED
BUTTON B CLICKED
```

`PRESSED` reports the current level. `CLICKED` reports a debounced edge and is
consumed by the library when used by a condition; `BUTTON STATUS` displays it
without consuming it. For sequence control use `IF_BUTTON` or `WAIT_BUTTON`.

The editor may also send the structured form `BUTTON READ A PRESSED` (or
`CLICKED`/`B`); it is equivalent to the short form.

## Wi-Fi and time

```text
WIFI BEGIN <ssid> <password>
WIFI STATUS
WIFI RSSI

TIME EPOCH
TIME FORMAT [gmtOffset]
```

After `WIFI BEGIN`, the bridge calls `MyDot::run()` continuously so automatic
Wi-Fi and MQTT recovery remains active. SSIDs and passwords containing spaces
are not supported by this simple whitespace-delimited protocol.

## Microeden cloud

```text
CLOUD BEGIN <deviceId> <token>
CLOUD SEND
CLOUD STATUS
CLOUD BUFFER <128..65535>
CLOUD SYNC <intervalMs>
CLOUD WRITE_TEXT <key> <value>
CLOUD WRITE_INT <key> <integer>
CLOUD WRITE_FLOAT <key> <number>
CLOUD WRITE_DOUBLE <key> <number>
CLOUD WRITE_BOOL <key> TRUE|FALSE
CLOUD ON_COMMAND <expected> [key]
CLOUD READ <key>
CLOUD READ_ONCE <key>
```

Within the continuous flow the Bridge may append `GOTO <label>` to
`CLOUD ON_COMMAND <expected>` to poll multiple commands without blocking the
other listeners. Without `GOTO`, the legacy cooperative wait is preserved.

`READ` returns the cached cloud state. `READ_ONCE` consumes a newly received
widget value and returns `CLOUD VALUE_ONCE empty` when no new value is pending.
`ON_COMMAND` uses the default `content` key, consumes a matching command from
the inbound cloud document and, during `RUN`, cooperatively waits until the
command arrives. The optional `key` argument remains accepted for backwards
compatibility, but the Bridge runtime supports `content`.

`CLOUD SYNC` installs the bridge telemetry callback, which publishes the bridge
name and current sequence length at the requested interval.

If MQTT is briefly unavailable, `CLOUD SEND` keeps pending values in memory
and the saved runtime continues. A later send can publish them after the
connection recovers.

## Cloud widgets

All widget wrappers exposed by `MyDot.h` are available through one command:

```text
WIDGET READ <type> <key>
WIDGET WRITE <type> <key> <value...>
```

`WIDGET WRITE` publishes the value immediately. If MQTT is disconnected, the
value stays queued for a later `WIDGET WRITE` or `CLOUD SEND` and the runtime
continues. An explicit `CLOUD SEND` after a successful write is harmless.

Supported types and values:

| Type | Read | Write |
| --- | --- | --- |
| `RAW` / `TEXT` | Cached text | Arbitrary text |
| `COLOR` | RGB triplet | `red green blue` |
| `MAP` | Cached coordinate text | `latitude longitude` or `TEXT <value>` |
| `LEVEL` | Integer | Numeric value, including decimals |
| `SLIDER` | Newly received integer (`-1` when empty) | Numeric value, including decimals |
| `SWITCH` | Boolean | `TRUE`/`FALSE` |
| `PUSHBUTTON` | Boolean | `TRUE`/`FALSE` |
| `LED` | Boolean | `TRUE`/`FALSE` |
| `PHOTO` | Cached text | Text |

Examples:

```text
EXEC WIDGET WRITE COLOR light 255 0 0
EXEC WIDGET WRITE SLIDER brightness 128
EXEC WIDGET WRITE LEVEL temperature 21.95
EXEC WIDGET READ SLIDER brightness
EXEC WIDGET READ MAP location
```

`LEVEL` and `SLIDER` writes accept both integers and decimal values. The
bridge publishes them as a JSON number, so a value produced by
`SENSOR_READ` (for example `21.95`) can be sent directly to a widget:

```text
ADD SENSOR_READ TEMPERATURE TEMP
ADD WIDGET WRITE LEVEL temperature $TEMP
SAVE
RUN
```

The typed `Level` and `Slider` read helpers still expose integer values when
reading dashboard input; decimal support applies to values written by runtime
blocks.

## Color conversion

```text
COLOR HEX_TO_RGB #00AAFF
COLOR RGB_TO_HEX 0 170 255
```

The first form prints `COLOR RGB r g b`; the second prints `COLOR HEX #RRGGBB`.

## SD card commands

The bridge initializes SD during `setup()`. `SD BEGIN` can retry
initialization. File paths may be absolute or relative according to the
selected board's SD library.

```text
SD BEGIN
SD EXISTS <path>
SD MKDIR <path>
SD TOUCH <path>
SD REMOVE <path>
SD WRITE <path> <text>
SD WRITE_LINE <path> <text>
SD APPEND <path> <text>
SD APPEND_LINE <path> <text>
SD OPEN <path> [r|w|a]
SD READ <path>
SD READ_B64 <path>
SD LIST [path]
```

`SD WRITE` replaces a text file and `SD APPEND` adds text at the end.
`WRITE_LINE` and `APPEND_LINE` add a newline automatically. `SD READ`
prints the contents between `BEGIN_SD_READ` and `END_SD_READ`. `SD LIST` lists
the entries in the selected directory (the root directory is used when the
path is omitted):

`SD MKDIR` creates a directory and `SD TOUCH` creates an empty file; both
operations fail when the target path already exists.

```text
EXEC SD WRITE /message.txt hello from MyDot
EXEC SD APPEND /message.txt !
EXEC SD APPEND_LINE /message.txt A complete line
EXEC SD READ /message.txt
EXEC SD LIST /
```

Directory output is delimited by `BEGIN_SD_LIST` and `END_SD_LIST`; each entry
is reported as `FILE <name> FILE size=<bytes>` or `FILE <name> DIR`.

`READ_B64` is the Dev Studio safe-transfer variant. It returns any file,
including binary data, Base64-encoded between `BEGIN_SD_READ_B64` and
`END_SD_READ_B64`.

The runtime file is always `/MyDot.run`. It contains a versioned
header, an XTEA-CTR encrypted command payload and a CRC32 checksum. The key is
compiled into the firmware, so this is protection for data at rest rather than
a secure key store. Credentials are stored in the runtime payload only when
you explicitly add commands such as `ADD WIFI BEGIN ...` or `ADD CLOUD BEGIN
...`; `EXEC` commands are not recorded.

## Complete sequence example

```text
CLEAR
ADD SET LIMIT 2200
ADD LABEL LOOP
ADD ANALOG_READ A0 SOIL
ADD SERIAL soil=$SOIL
ADD IF_VAR SOIL > $LIMIT GOTO DRY
ADD BRIGHTNESS 32
ADD GOTO WAIT
ADD LABEL DRY
ADD BRIGHTNESS 255
ADD LABEL WAIT
ADD WAIT_BUTTON B CLICKED 60000
ADD REPEAT 3
ADD PIXELS_RANDOM
ADD DELAY 250
ADD ENDREPEAT
ADD GOTO LOOP
SAVE
RUN
```

Use `STOP` to terminate this sequence and switch outputs off.
