# RCSA exercise

A MONet-based tool for configuring and running simple RCSA simulations.

> [!IMPORTANT]
> Docker is required to run this project.

## Quick start

Pull the published Docker image and run a simulation:

### Unix (Linux, macOS, and WSL)

```bash
docker pull ghcr.io/mirkozeta/monet-rcsa-exercise-tool:0.1.1

mkdir -p results

docker run --rm \
  --user "$(id -u):$(id -g)" \
  -v "$(pwd)/results:/results" \
  ghcr.io/mirkozeta/monet-rcsa-exercise-tool:0.1.1 \
  Germany-14nodes 1 3 1 /results/run-001
```

### Windows (PowerShell)

```powershell
docker pull ghcr.io/mirkozeta/monet-rcsa-exercise-tool:0.1.1

New-Item -ItemType Directory -Force results

docker run --rm `
  --mount "type=bind,source=$($PWD.Path)\results,target=/results" `
  ghcr.io/mirkozeta/monet-rcsa-exercise-tool:0.1.1 `
  Germany-14nodes 1 3 1 /results/run-001
```

## Build locally

Build the Docker image from the repository root:

### Unix (Linux, macOS, and WSL)

```bash
docker build -t monet-rcsa-exercise-tool .

mkdir -p results

docker run --rm \
  --user "$(id -u):$(id -g)" \
  -v "$(pwd)/results:/results" \
  monet-rcsa-exercise-tool \
  Germany-14nodes 1 3 1 /results/run-001
```

### Windows (PowerShell)

```powershell
docker build -t monet-rcsa-exercise-tool .

New-Item -ItemType Directory -Force results

docker run --rm `
  --mount "type=bind,source=$($PWD.Path)\results,target=/results" `
  monet-rcsa-exercise-tool `
  Germany-14nodes 1 3 1 /results/run-001
```

## Project structure

```text
.
├── .github
│   └── workflows
│       └── release.yml
├── gui/ (TO DO)
│   └── README.md
├── simulator
│   ├── main.cpp
│   ├── algorithms.hpp
│   ├── simulator.hpp
│   └── resources
│       ├── SSMF_C.json
│       ├── bitrates.json
│       ├── demands
│       └── topologies
├── CMakeLists.txt
├── Dockerfile
└── README.md
```

`simulator.hpp` is the bundled MONet simulator library. Exercise-specific
command-line handling is in `main.cpp`, while RCSA implementations belong in
`algorithms.hpp`. The `gui` directory is reserved for the future graphical
interface and does not currently contain an implementation.

## Command-line interface

The executable expects five positional arguments in this order:

```text
monet-exercise NETWORK ROUTING K RCSA OUTPUT_FOLDER
```

- `NETWORK`: topology name, currently `Germany-14nodes`
- `ROUTING`: `1` for shortest paths or `2` for disjoint paths
- `K`: positive number of candidate paths
- `RCSA`: `1` for First Fit
- `OUTPUT_FOLDER`: directory in which the CSV reports are created

Example:

```bash
./build/monet-exercise Germany-14nodes 1 3 1 ./results/run-001
```

Exit code `0` means the simulation completed successfully. Invalid command-line
input returns `2`; a simulation or file error returns `3`.

## Build without Docker

Requirements: CMake 3.20 or newer and a C++20 compiler.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

## Generate resources with TopoLib

Use this tool to extend the available simulation inputs with additional
networks. It converts a TopoLib network into MONet-compatible topology and
traffic-demand JSON files.

> [!NOTE]
> The resource-generation tool is not included in the Docker image. Run the
> commands below from the repository root.

Install its dependency and run it:

```bash
pip install -r requirements.txt
python simulator/resources/topolib_extract.py --list-topologies
python simulator/resources/topolib_extract.py Germany-14nodes
```

The command writes the topology to
`simulator/resources/topologies/<NETWORK>.json` and demands to
`simulator/resources/demands/<NETWORK>_demands.json`.
