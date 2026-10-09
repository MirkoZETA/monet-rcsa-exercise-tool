#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#include "algorithms.hpp"

namespace {

void printUsage(std::ostream &stream, const char *programName) {
  stream
      << "Usage: " << programName
      << " <network-name> <route-set> <number-of-paths> <route-ordering>"
         " <rcsa-algorithm> <output-folder>\n"
      << "  route-set:       1 = shortest paths, 2 = disjoint paths\n"
      << "  route-ordering:  1 = shortest first, 2 = longest first,\n"
      << "                   3 = least loaded first, 4 = most loaded first\n"
      << "  rcsa-algorithm:  1 = FirstFit (FF-PIA)\n"
      << "  output-folder:   directory for generated reports\n";
}

std::filesystem::path executableDirectory(const char *programName) {
  // In the Docker image, application resources are stored beside the
  // executable. /proc/self/exe provides the executable's real location even
  // when it was launched through PATH or a symbolic link.
  std::error_code error;
  const std::filesystem::path executablePath =
      std::filesystem::read_symlink("/proc/self/exe", error);
  if (!error && !executablePath.empty()) {
    return executablePath.parent_path();
  }

  // Fallback for environments without /proc.
  const std::filesystem::path fallback =
      std::filesystem::absolute(programName, error);
  if (error) {
    throw std::runtime_error("could not determine the executable directory");
  }
  return fallback.parent_path();
}

void requireRegularFile(const std::filesystem::path &path,
                        const std::string &description) {
  std::error_code error;
  const bool isRegularFile = std::filesystem::is_regular_file(path, error);
  if (error || !isRegularFile) {
    throw std::runtime_error(description + " was not found: " + path.string());
  }
}

int runSimulation(const CommandLineOptions &options, const char *programName) {
  // These are internal application assets bundled in the same image as the
  // executable. Users select the scenario by name and never manage the files.
  const std::filesystem::path resourceDirectory =
      executableDirectory(programName) / "resources";
  const std::filesystem::path topologyPath =
      resourceDirectory / "topologies" / (options.networkName + ".json");
  const std::filesystem::path demandsPath =
      resourceDirectory / "demands" /
      (options.networkName + "_demands.json");
  const std::filesystem::path bitratesPath =
      resourceDirectory / "bitrates.json";
  const std::filesystem::path fiberPath = resourceDirectory / "SSMF_C.json";

  requireRegularFile(topologyPath, "Topology file");
  requireRegularFile(demandsPath, "Demands file");
  requireRegularFile(bitratesPath, "Bit-rate file");
  requireRegularFile(fiberPath, "Fiber specification file");

  Simulator simulator(topologyPath.string(), fiberPath.string(),
                      bitratesPath.string(), demandsPath.string(),
                      mt::SimulationMode::GN);

  simulator.setNumberOfPeriods(1);
  simulator.setAutoFeasibilityCheck(true);
  simulator.setOutputFolder(options.outputFolder);

  switch (options.routeSet) {
  case RouteSet::ShortestPaths:
    simulator.setPathsShortest(options.numberOfPaths);
    break;
  case RouteSet::DisjointPaths:
    simulator.setPathsDisjoint(options.numberOfPaths);
    break;
  }

  routeOrdering = options.routeOrder;

  switch (options.spectrumAllocationAlgorithm) {
  case SpectrumAllocationAlgorithm::FirstFit:
    USE_ALLOC_FUNCTION(FirstFit, simulator);
    break;
  }

  simulator.init();
  simulator.run(true);
  return 0;
}

} // namespace

int main(int argc, char *argv[]) {
  // Flush each insertion so a GUI, API, or `docker logs` receives output while
  // the simulation is still running rather than in buffered batches.
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  try {
    const CommandLineOptions options = parseCommandLine(argc, argv);
    return runSimulation(options, argv[0]);
  } catch (const std::invalid_argument &error) {
    std::cerr << "Error: " << error.what() << "\n\n";
    printUsage(std::cerr, argv[0]);
    return 2;
  } catch (const std::exception &error) {
    std::cerr << "Simulation failed: " << error.what() << '\n';
    return 3;
  }
}
