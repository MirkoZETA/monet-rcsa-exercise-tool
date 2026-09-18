# RCSA exercise

## Project structure

```text
.
├── CMakeLists.txt
├── Dockerfile
├── README.md
└── simulator
    ├── main.cpp
    ├── algorithms.hpp
    ├── simulator.hpp
    └── resources
        ├── SSMF_C.json
        ├── bitrates.json
        ├── demands
        └── topologies
```

`simulator.hpp` is the bundled MONet simulator library. Exercise-specific
command-line handling is in `main.cpp`, while RCSA implementations belong in
`algorithms.hpp`.

## Generate resources with TopoLib

The script converts a TopoLib network into MONet-compatible topology and 
traffic-demand JSON files. The script is not included in the Docker image. 
To Install its dependency in the Python environment and run it from the repository root:

```bash
pip install -r requirements.txt
python simulator/resources/topolib_extract.py --list-topologies
python simulator/resources/topolib_extract.py Germany-14nodes
```

The command writes the topology to
`simulator/resources/topologies/<NETWORK>.json` and demands to
`simulator/resources/demands/<NETWORK>_demands.json`.

## Build locally

Requirements: CMake 3.20 or newer and a C++20 compiler.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

## Command-line interface

To run a simulation, the executable expects five positional arguments in this order:

```text
monet-exercise NETWORK ROUTING K RCSA OUTPUT_FOLDER
```

- `NETWORK`: Topology name, currently `Germany-14nodes`
- `ROUTING`: `1` for shortest paths or `2` for disjoint paths
- `K`: number of candidate paths
- `RCSA`: `1` for First Fit
- `OUTPUT_FOLDER`: directory in which the CSV reports are created

Example:

```bash
./build/monet-exercise Germany-14nodes 1 3 1 ./results/run-001
```

Exit code `0` means the simulation completed successfully. Invalid command-line
input returns `2`; a simulation or file error returns `3`.

## Build and run with Docker

```bash
docker build -t monet-exercise .
mkdir -p results
docker run --rm \
  -v "$(pwd)/results:/results" \
  monet-exercise Germany-14nodes 1 3 1 /results/run-001
```

The mounted `results` directory keeps generated files on the host.
