#pragma once

#include "simulator.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <numeric>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

// ── 1. Command-line option types ─────────────────────────────────────────────

enum class RouteSet : int {
  ShortestPaths = 1,
  DisjointPaths = 2,
};

enum class RouteOrder : int {
  ShortestFirst = 1,
  LongestFirst = 2,
  LeastLoadedFirst = 3,
  MostLoadedFirst = 4,
};

enum class SpectrumAllocationAlgorithm : int {
  FirstFit = 1,
  BestFit = 2,
  RandomFit = 3,
  LeastUsed = 4,
  MostUsed = 5,
  RelativeCapacityLoss = 6,
};

// ── 2. Command-line parsing helpers ──────────────────────────────────────────

struct CommandLineOptions {
  std::string networkName;
  RouteSet routeSet;
  int numberOfPaths;
  RouteOrder routeOrder;
  SpectrumAllocationAlgorithm spectrumAllocationAlgorithm;
  std::filesystem::path outputFolder;
};

inline int parsePositiveInteger(const std::string &value,
                                const std::string &argumentName) {
  int parsedValue = 0;
  const auto [end, error] =
      std::from_chars(value.data(), value.data() + value.size(), parsedValue);
  if (error != std::errc{} || end != value.data() + value.size() ||
      parsedValue <= 0) {
    throw std::invalid_argument(argumentName + " must be a positive integer");
  }
  return parsedValue;
}

inline RouteSet parseRouteSet(const std::string &value) {
  switch (parsePositiveInteger(value, "route-set")) {
  case static_cast<int>(RouteSet::ShortestPaths):
    return RouteSet::ShortestPaths;
  case static_cast<int>(RouteSet::DisjointPaths):
    return RouteSet::DisjointPaths;
  default:
    throw std::invalid_argument(
        "route-set must be 1 (shortest) or 2 (disjoint)");
  }
}

inline RouteOrder parseRouteOrder(const std::string &value) {
  switch (parsePositiveInteger(value, "route-ordering")) {
  case static_cast<int>(RouteOrder::ShortestFirst):
    return RouteOrder::ShortestFirst;
  case static_cast<int>(RouteOrder::LongestFirst):
    return RouteOrder::LongestFirst;
  case static_cast<int>(RouteOrder::LeastLoadedFirst):
    return RouteOrder::LeastLoadedFirst;
  case static_cast<int>(RouteOrder::MostLoadedFirst):
    return RouteOrder::MostLoadedFirst;
  default:
    throw std::invalid_argument(
        "route-ordering must be 1 (shortest first), 2 (longest first), "
        "3 (least loaded first), or 4 (most loaded first)");
  }
}

inline SpectrumAllocationAlgorithm
parseSpectrumAllocationAlgorithm(const std::string &value) {
  switch (parsePositiveInteger(value, "rcsa-algorithm")) {
  case static_cast<int>(SpectrumAllocationAlgorithm::FirstFit):
    return SpectrumAllocationAlgorithm::FirstFit;
  default:
    throw std::invalid_argument("rcsa-algorithm must be 1 (FirstFit)");
  }
}

inline CommandLineOptions parseCommandLine(int argc, char *argv[]) {
  if (argc != 7) {
    throw std::invalid_argument("expected exactly six arguments");
  }

  CommandLineOptions options{
      .networkName = argv[1],
      .routeSet = parseRouteSet(argv[2]),
      .numberOfPaths = parsePositiveInteger(argv[3], "number-of-paths"),
      .routeOrder = parseRouteOrder(argv[4]),
      .spectrumAllocationAlgorithm = parseSpectrumAllocationAlgorithm(argv[5]),
      .outputFolder = std::filesystem::path(argv[6]),
  };

  if (options.outputFolder.empty()) {
    throw std::invalid_argument("output-folder cannot be empty");
  }

  return options;
}

// ── 3. Route-ordering helpers ────────────────────────────────────────────────

// Set once from the command line before the simulation starts.
inline RouteOrder routeOrdering = RouteOrder::ShortestFirst;

// Calculate the representative slot utilization of a given route
inline double routeUtilization(const mt::Route &route) {
  if (route.empty()) {
    return 1.0;
  }

  const auto &firstFiber = route.front()->getFiber(0);
  std::uint64_t occupiedSlots = 0;
  std::uint64_t totalSlots = 0;

  for (const auto band : firstFiber.getFiberSpec()->getBands()) {
    const std::size_t slotCount = firstFiber.getSlots(0, band, 0).size();
    totalSlots += slotCount;

    for (std::size_t slot = 0; slot < slotCount; ++slot) {
      for (const auto &link : route) {
        if (link->getFiber(0).getSlots(0, band, 0).at(slot) != 0) {
          ++occupiedSlots;
          break; // One occupied link is enough, check the next slot.
        }
      }
    }
  }

  if (totalSlots == 0) {
    return 1.0;
  }
  return static_cast<double>(occupiedSlots) / static_cast<double>(totalSlots);
}

/**
 * Returns route indices sorted by requested ordering strategy.
 */
inline std::vector<std::size_t>
orderRouteIndices(const std::vector<mt::Route> &routes) {
  const std::size_t routeCount = routes.size();

  // Create initial indices: [0, 1, 2, ..., routeCount - 1]
  std::vector<std::size_t> indices(routeCount);
  std::iota(indices.begin(), indices.end(), std::size_t{0});

  // 1. Direct return for simple length-based orders
  if (routeOrdering == RouteOrder::ShortestFirst) {
    return indices;
  }
  if (routeOrdering == RouteOrder::LongestFirst) {
    std::reverse(indices.begin(), indices.end());
    return indices;
  }

  if (routeOrdering != RouteOrder::LeastLoadedFirst &&
      routeOrdering != RouteOrder::MostLoadedFirst) {
    throw std::invalid_argument("unknown route ordering");
  }

  // 2. Precalculate utilization for load-based orders
  std::vector<double> utilizations(routeCount);
  for (std::size_t i = 0; i < routeCount; ++i) {
    utilizations[i] = routeUtilization(routes[i]);
  }

  // 3. Sort indices directly, preserving shortest-first order for ties.
  const bool leastLoaded = routeOrdering == RouteOrder::LeastLoadedFirst;
  std::stable_sort(indices.begin(), indices.end(),
                   [&utilizations, leastLoaded](std::size_t a, std::size_t b) {
                     return leastLoaded ? utilizations[a] < utilizations[b]
                                        : utilizations[a] > utilizations[b];
                   });

  return indices;
}
