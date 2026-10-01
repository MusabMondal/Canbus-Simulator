# ECU CAN Telemetry Simulator

A small C99 project for exploring how vehicle data can be encoded into CAN-style messages, delivered to a receiving ECU, and recorded for analysis. Everything runs locally in memory; no car, CAN adapter, or embedded board is required.

The simulator starts with the engine on. Every simulation step represents 10 milliseconds: it updates vehicle values, toggles braking every second, builds three messages, delivers them in identifier order, and logs the receiver's reconstructed state.

## Build and run

You need a C compiler and Make. Python 3 is required only for the telemetry test; the plotting notebook also needs pandas, matplotlib, NumPy, and Jupyter.

```sh
make
make run
```

Press Ctrl+C to stop. Telemetry is written to `telemetry_log.csv` and the file is closed on normal shutdown or Ctrl+C. Running again replaces that file.

For a repeatable ten-second simulation without waiting or printing each step:

```sh
./ecu_sim --steps 1000 --fast --quiet --output build/telemetry.csv
```

Options:

| Option | Behavior |
| --- | --- |
| `--steps N` | Stop after N steps; N must be positive. Default: run continuously. |
| `--fast` | Skip the delay between steps. |
| `--quiet` | Suppress per-step console output. |
| `--output PATH` | Select the CSV destination. Default: `telemetry_log.csv`. |
| `--help` | Print usage. |

Without `--fast`, the program sleeps for 10 milliseconds after each step. This is approximate pacing, not a real-time timing guarantee.

## Message flow

`vehicle model → encode frames → queue → sort by ID → validate and decode → CSV`

| ID | Payload bytes | Contents |
| --- | --- | --- |
| `0x100` | 6 | Big-endian RPM, gear, engine/brake flags, sequence, XOR checksum |
| `0x200` | 6 | Speed, brake pressure, throttle, reserved byte, sequence, XOR checksum |
| `0x300` | 7 | Temperature, oil pressure, battery voltage, diagnostic flags, reserved byte, sequence, XOR checksum |

Lower identifiers are delivered first. Each message type has its own rolling 8-bit sequence counter. The receiver checks the identifier, payload length, checksum, and sequence before applying data. A sequence gap rejects that message and resynchronizes tracking so later messages can be accepted. The first valid message establishes the sequence baseline.

Diagnostic flags cover checksum errors, sequence errors, excessive RPM, high temperature, low oil pressure, and low battery voltage. Communication errors remain latched in the receiver. The timeout flag is reserved; timeout detection is not implemented.

The CSV preserves the original columns: `Timestamp`, `RPM`, `Speed`, `Throttle`, `BrakePressure`, `Gear`, `OilPressure`, `EngineTemp`, `BatteryVoltage`, and `EngineOn`. Timestamps are simulated milliseconds; battery voltage uses tenths of a volt.

## Project layout

- `src/simulator.c`: command-line options, shutdown, simulation loop, and CSV output.
- `src/vehicle_model.c` / `.h`: vehicle state, simplified acceleration/braking rules, sensors, and diagnostics.
- `src/can_transport.c` / `.h`: payload codecs, queue, priority ordering, and integrity validation.
- `tests/`: message-level checks and a complete telemetry run.
- `analysis/plot_telemetry.ipynb`: plots RPM, speed, throttle/braking, and temperature.
- `analysis/sample_telemetry.png`: preserved example plot.
- `data/sample_telemetry.csv`: preserved example recording.
- `docs/can_protocol.md`: background notes on CAN.

The source files were renamed from `main`, `ecu`, and `can` to describe their responsibilities. Build outputs now live in `build/`; the executable remains `ecu_sim`.

## Verify changes

```sh
make test
make clean
```

Tests cover round-trip message delivery, priority ordering, queue overflow, checksum rejection, duplicate and missing messages, sequence recovery and rollover, malformed payload length, low-value arithmetic, temperature diagnostics, and a 1,000-step CSV run through acceleration and braking.

## Plot telemetry

After running the simulator with its default CSV destination:

```sh
cd analysis
jupyter notebook plot_telemetry.ipynb
```

The notebook uses the generated CSV when present, otherwise the bundled sample. It saves `ecu_telemetry_analysis.png` in the notebook's working directory.

## Scope

This is an educational application-level simulation. Priority ordering uses a sorted queue rather than electrical, bit-by-bit arbitration. The XOR checksum is an application payload check, not CAN's native CRC. Physical signaling, ACKs, bit stuffing, retransmission, hardware interfaces, and standardized diagnostic services are not modeled. Vehicle values follow simple rules rather than a calibrated physics model.

Original project by Pedro Henrique Bonifácio da Rosa. The original README stated MIT licensing; this checkout does not include a separate license file.
