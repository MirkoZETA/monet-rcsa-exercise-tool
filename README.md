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
  Germany-14nodes 1 3 1 1 1 /results/run-001
```

### Windows (PowerShell)

```powershell
docker pull ghcr.io/mirkozeta/monet-rcsa-exercise-tool:0.1.1

New-Item -ItemType Directory -Force results

docker run --rm `
  --mount "type=bind,source=$($PWD.Path)\results,target=/results" `
  ghcr.io/mirkozeta/monet-rcsa-exercise-tool:0.1.1 `
  Germany-14nodes 1 3 1 1 1 /results/run-001
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
  Germany-14nodes 1 3 1 1 1 /results/run-001
```

### Windows (PowerShell)

```powershell
docker build -t monet-rcsa-exercise-tool .

New-Item -ItemType Directory -Force results

docker run --rm `
  --mount "type=bind,source=$($PWD.Path)\results,target=/results" `
  monet-rcsa-exercise-tool `
  Germany-14nodes 1 3 1 1 1 /results/run-001
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
│   ├── utils.hpp
│   ├── simulator.hpp
│   └── resources
│       ├── SSMF_C.json
│       ├── bitrates
│       │   ├── all.json
│       │   ├── high.json
│       │   └── low.json
│       ├── demands
│       └── topologies
├── CMakeLists.txt
├── Dockerfile
└── README.md
```

`simulator.hpp` is the bundled MONet simulator library. Shared command-line
parsing and route-ordering helpers are in `utils.hpp`, simulation setup is in
`main.cpp`, and spectrum-allocation implementations belong in `algorithms.hpp`.
The `gui` directory is reserved for the future graphical interface and does not
currently contain an implementation.

## Command-line interface

The executable expects seven positional arguments in this order:

```text
monet-exercise NETWORK ROUTE_SET K ROUTE_ORDERING BITRATE_SET SPECTRUM_ALLOCATION OUTPUT_FOLDER
```

- `NETWORK`: topology name, currently `Germany-14nodes`
- `ROUTE_SET`: `1` for shortest paths or `2` for disjoint paths
- `K`: positive number of candidate paths
- `ROUTE_ORDERING`: candidate evaluation order:
  - `1`: shortest to longest
  - `2`: longest to shortest
  - `3`: least loaded to most loaded
  - `4`: most loaded to least loaded
- `BITRATE_SET`: available bitrates:
  - `1`: all bitrates, from 100 to 600 Gbps
  - `2`: low bitrates, from 100 to 300 Gbps
  - `3`: high bitrates, from 350 to 600 Gbps
- `SPECTRUM_ALLOCATION`: `1` for First Fit
- `OUTPUT_FOLDER`: directory in which the CSV reports are created

Example:

```bash
./build/monet-exercise Germany-14nodes 1 3 1 1 1 ./results/run-001
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
