"""Export a topolib topology and its related simulation inputs.

The generated simulation inputs are written to the directories consumed by
MONet:

  topologies/<topology>.json
  demands/<topology>_demands.json

Examples:

  python topolib_extract.py Germany-14nodes
  python topolib_extract.py --list-topologies
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import TYPE_CHECKING

if TYPE_CHECKING:
  from topolib.topology import Topology


SCRIPT_DIR = Path(__file__).resolve().parent
TOPOLOGIES_DIR = SCRIPT_DIR / "topologies"
DEMANDS_DIR = SCRIPT_DIR / "demands"


def export_topology(topology: Topology, output_path: Path) -> None:
  """Export and normalize a topology for use by MONet.

  The normalization preserves the existing export behavior:

  * links without slot or fiber information receive one SSMF fiber;
  * node ``name`` fields are renamed to ``label``.
  """

  topology.export_to_json(str(output_path))

  with output_path.open("r", encoding="utf-8") as file:
    topology_data = json.load(file)

  for link in topology_data.get("links", []):
    if "slots" not in link and "fibers" not in link:
      link["fibers"] = {"SSMF": 1}

  for node in topology_data.get("nodes", []):
    if "name" in node:
      node["label"] = node.pop("name")

  with output_path.open("w", encoding="utf-8") as file:
    json.dump(topology_data, file, indent=4)
    file.write("\n")


def list_available_topologies() -> list[str]:
  """Return the names of all default TopoLib topologies."""

  from topolib.topology import Topology

  return [entry["name"] for entry in Topology.list_available_topologies()]


def export_files(topology_name: str) -> tuple[Path, Path]:
  """Load a default topology and export MONet topology and demand inputs."""

  from topolib.analysis import TrafficMatrix
  from topolib.topology import Topology

  topology = Topology.load_default_topology(topology_name)

  TOPOLOGIES_DIR.mkdir(parents=True, exist_ok=True)
  DEMANDS_DIR.mkdir(parents=True, exist_ok=True)

  topology_path = TOPOLOGIES_DIR / f"{topology_name}.json"
  demand_path = DEMANDS_DIR / f"{topology_name}_demands.json"

  export_topology(topology, topology_path)

  matrix = TrafficMatrix.ram(topology)
  TrafficMatrix.to_json(matrix, topology, str(demand_path))

  return topology_path, demand_path


def build_parser() -> argparse.ArgumentParser:
  """Create the command-line argument parser."""

  parser = argparse.ArgumentParser(
    description=(
      "Export a default topolib topology and RAM demands. "
      "Use --list-topologies to view available names."
    )
  )
  parser.add_argument(
    "topology_name",
    nargs="?",
    metavar="TOPOLOGY",
    help="Default topolib topology name, for example Spain-21nodes. Use --list-topologies to see all available names.",
  )
  parser.add_argument(
    "-l",
    "--list-topologies",
    action="store_true",
    help="List all available default TopoLib topologies and exit.",
  )
  return parser


def main() -> int:
  """Run the export command."""

  parser = build_parser()
  arguments = parser.parse_args()

  if arguments.list_topologies:
    topologies = list_available_topologies()
    if not topologies:
      print("No default topologies were found in topolib.")
      return 0

    print("Available topologies:")
    for topology_name in topologies:
      print(f"  {topology_name}")
    return 0

  if arguments.topology_name is None:
    parser.error("a TOPOLOGY name is required unless using --list-topologies")

  try:
    output_paths = export_files(arguments.topology_name)
  except FileNotFoundError as error:
    parser.error(
      f"topology {arguments.topology_name!r} was not found in topolib: {error}"
    )
  except Exception as error:
    parser.error(f"export failed: {error}")

  print("Exported files:")
  for output_path in output_paths:
    print(f"  {output_path}")

  return 0


if __name__ == "__main__":
  raise SystemExit(main())
