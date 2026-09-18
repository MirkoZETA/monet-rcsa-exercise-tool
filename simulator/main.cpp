#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

#include "algorithms.hpp"

namespace {

enum class RoutingOption : int {
  ShortestPaths = 1,
  DisjointPaths = 2,
};

enum class RcsaAlgorithm : int {
  FirstFit = 1,
};

struct CommandLineOptions {
  std::string networkName;
  RoutingOption routingOption;
  int numberOfPaths;
  RcsaAlgorithm rcsaAlgorithm;
  std::filesystem::path outputFolder;
};

void printUsage(std::ostream &stream, const char *programName) {
  stream
      << "Usage: " << programName
      << " <network-name> <routing-option> <number-of-paths> <rcsa-algorithm>"
         " <output-folder>\n"
      << "  routing-option: 1 = shortest paths, 2 = disjoint paths\n"
      << "  rcsa-algorithm:  1 = FirstFit (FF-PIA)\n"
      << "  output-folder:   directory for generated reports\n";
}

int parsePositiveInteger(const std::string &value,
                         const std::string &argumentName) {
  const bool containsOnlyDigits =
      !value.empty() &&
      std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return std::isdigit(character) != 0;
      });

  if (!containsOnlyDigits) {
    throw std::invalid_argument(argumentName +
                                " must contain only decimal digits");
  }

  try {
    const long long parsedValue = std::stoll(value);
    if (parsedValue <= 0 ||
        parsedValue > std::numeric_limits<int>::max()) {
      throw std::invalid_argument(argumentName +
                                  " must be a positive integer");
    }
    return static_cast<int>(parsedValue);
  } catch (const std::out_of_range &) {
    throw std::invalid_argument(argumentName + " is outside the valid range");
  }
}

RoutingOption parseRoutingOption(const std::string &value) {
  switch (parsePositiveInteger(value, "routing-option")) {
  case static_cast<int>(RoutingOption::ShortestPaths):
    return RoutingOption::ShortestPaths;
  case static_cast<int>(RoutingOption::DisjointPaths):
    return RoutingOption::DisjointPaths;
  default:
    throw std::invalid_argument(
        "routing-option must be 1 (shortest) or 2 (disjoint)");
  }
}

RcsaAlgorithm parseRcsaAlgorithm(const std::string &value) {
  switch (parsePositiveInteger(value, "rcsa-algorithm")) {
  case static_cast<int>(RcsaAlgorithm::FirstFit):
    return RcsaAlgorithm::FirstFit;
  default:
    throw std::invalid_argument("rcsa-algorithm must be 1 (FirstFit)");
  }
}

void validateNetworkName(const std::string &networkName) {
  const bool isValid =
      !networkName.empty() &&
      std::all_of(networkName.begin(), networkName.end(),
                  [](unsigned char character) {
                    return std::isalnum(character) != 0 || character == '-' ||
                           character == '_';
                  });

  if (!isValid) {
    throw std::invalid_argument(
        "network-name may contain only letters, digits, '-' and '_'");
  }
}

CommandLineOptions parseCommandLine(int argc, char *argv[]) {
  if (argc != 6) {
    throw std::invalid_argument("expected exactly five arguments");
  }

  CommandLineOptions options{
      .networkName = argv[1],
      .routingOption = parseRoutingOption(argv[2]),
      .numberOfPaths = parsePositiveInteger(argv[3], "number-of-paths"),
      .rcsaAlgorithm = parseRcsaAlgorithm(argv[4]),
      .outputFolder = std::filesystem::path(argv[5]),
  };

  validateNetworkName(options.networkName);
  if (options.outputFolder.empty()) {
    throw std::invalid_argument("output-folder cannot be empty");
  }

  return options;
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

  switch (options.routingOption) {
  case RoutingOption::ShortestPaths:
    simulator.setPathsShortest(options.numberOfPaths);
    break;
  case RoutingOption::DisjointPaths:
    simulator.setPathsDisjoint(options.numberOfPaths);
    break;
  }

  switch (options.rcsaAlgorithm) {
  case RcsaAlgorithm::FirstFit:
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
