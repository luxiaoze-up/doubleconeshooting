# AGENTS.md

## Scope and supported environment

These instructions apply to the entire repository.

- The deployment and runtime target is **Ubuntu 24.04 x86-64**.
- Use GCC/Clang, CMake, POSIX sockets, systemd, Python 3 and Qt5/PyQt5.
- Keep one build tree at `build/`; never commit generated CMake output.
- The vendor motion library used by the project is `lib/libLTSMC.so`.
- Do not add alternate-OS build scripts, packaging output or conditional compatibility branches.

## Project architecture

DoubleConeShooting is a Tango Controls distributed control system.

1. `gui/` contains the operator applications. The primary application is `gui/vacuum_chamber_gui`; `gui/vacuum_system_gui` is the dedicated vacuum UI; `gui/six_dof_debug_gui` directly diagnoses the Stewart platform.
2. `src/system_services/` contains coordination services such as interlock.
3. `src/device_services/` contains Tango device servers for motion controllers, encoder acquisition, six-DOF, large stroke, auxiliary support, reflection imaging and vacuum.
4. `src/common/` contains shared configuration, Tango wrappers, PLC communication, kinematics and encoder acquisition.
5. `src/drivers/` and `include/drivers/` wrap vendor hardware APIs. Treat vendor headers and binary libraries as external interfaces.
6. `config/` is the source of runtime addresses, Tango properties, axis mapping, IO mapping and safety parameters.
7. `scripts/` contains registration, startup, diagnostics and tests. `tools/` contains simulators and offline analysis tools.

The normal request path is GUI → Tango `DeviceProxy` → device server → driver/protocol → hardware. Avoid bypassing Tango in production code unless the component is explicitly a direct diagnostic tool.

## Build and environment setup

Use a virtual environment for Python work:

```bash
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -U pip
python -m pip install -r requirements.txt
python -m pip install -r gui/requirements.txt
python -m pip install -r gui/vacuum_system_gui/requirements.txt
python -m pip install -r gui/six_dof_debug_gui/requirements.txt
```

Configure and build C++ from the repository root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

Use `-DCMAKE_BUILD_TYPE=Release` for deployment builds. If the compiler, Tango, Qt or a vendor SDK changes, regenerate `build/` instead of reusing a stale `CMakeCache.txt`.

## Runtime workflow

Before starting a real system, export the correct database endpoint and verify it:

```bash
export TANGO_HOST=127.0.0.1:10000
tango_admin --ping-database
```

The expected startup sequence is:

1. MariaDB and Tango Database;
2. Tango Starter;
3. device registration when configuration changed;
4. motion controller, encoder and dependent device services;
5. interlock and vacuum system;
6. GUI clients.

Useful commands:

```bash
python3 scripts/register_devices.py --config config/devices_config.json --force
python3 scripts/start_servers.py
python3 gui/vacuum_chamber_gui/main.py
```

`scripts/start_servers.py` currently includes `VacuumSystem`. Use `scripts/start_vacuum_system.sh` only when the general launcher is not running and the vacuum service must be managed independently; do not start the same instance twice.

## Coding conventions

### C++

- Use C++17 and RAII; make thread ownership and shutdown explicit.
- Preserve Tango command/attribute names and device property names unless a coordinated schema migration is requested.
- Keep blocking hardware IO out of GUI threads and protect shared device state with the existing synchronization model.
- Report protocol and driver failures through both logs and Tango state/status; do not silently turn a real failure into simulated success.
- Keep protocol-specific logic in common communication classes or drivers, not duplicated across device services.
- Do not edit `include/nlohmann/json.hpp` or vendor API declarations merely to satisfy formatting.

### Python

- Use Python 3, UTF-8, `pathlib` for repository paths and `logging` for long-running services.
- Resolve paths from `__file__` or the repository root; do not introduce machine-specific absolute paths.
- Keep PyQt work on the GUI thread and use the existing worker/signal pattern for Tango or PLC calls.
- Importing a module must not connect hardware, move an axis, switch power or create background processes.

### Shell and CMake

- Shell scripts must use Bash and quote variable expansions.
- Prefer `cmake -S . -B build` and `cmake --build build` over direct Makefile assumptions.
- System configuration scripts must be idempotent and must state which `/etc` file or systemd unit they change.

## Configuration rules

- `config/devices_config.json` is the main source for Tango registration and device properties.
- `config/system_config.json` controls shared system settings and simulation mode.
- `config/vacuum_system_config.json` controls PLC protocol, polling and vacuum safety thresholds.
- Preserve the existing camelCase property names consumed by C++ and Python. Search all readers before renaming a key.
- Never commit credentials, private keys, database dumps, local hostnames or operator-specific paths.
- Treat changes to controller IPs, axis numbers, encoder channels, active-low IO, limits and timeout values as hardware-impacting changes that require explicit review.

## Hardware safety

This repository can move mechanisms and switch pumps, valves, lights, brakes and drive power.

- Default to read-only checks or simulation when hardware authorization is unclear.
- Before a real motion test, confirm the target device, controller IP, axis mapping, units, direction, limits, speed, brake state and emergency-stop availability.
- Use small displacements and low speeds for first motion after a code or configuration change.
- Never weaken interlocks, limit checks, brake sequencing or fault transitions just to make a test pass.
- Tests that instantiate `tango.DeviceProxy` or load `libLTSMC.so` may be integration tests even if stored under `scripts/unit_test/`; inspect them before running.
- Do not run unattended hardware tests or simultaneous test processes against the same controller.

## Validation expectations

Run the narrowest relevant checks first, then expand:

```bash
python3 -m compileall -q gui scripts tools
scripts/run_tests.sh --unit -v
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

For documentation-only work, verify links and commands with `rg`. For configuration changes, parse the JSON and run the matching check script. For hardware-facing changes, report which checks were simulation-only and which require an Ubuntu device host or real equipment.

If a build cannot run because Tango, Qt, open62541, Snap7 or a vendor SDK is missing, report the exact missing dependency; do not add placeholder headers or fake successful results.

## Repository hygiene

- Do not commit `build/`, `dist/`, `__pycache__/`, `.venv/`, logs, reports, coverage output, editor metadata or generated binaries.
- Preserve unrelated user changes in a dirty worktree.
- Keep README and `docs/编译和测试指南.md` aligned with actual commands whenever build or startup behavior changes.
- Summarize deleted artifacts and any remaining environment-dependent verification in the handoff.
