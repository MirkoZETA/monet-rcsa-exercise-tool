#pragma once

#include "simulator.hpp"
#include <algorithm>
#include <cmath>
#include <deque>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

/**
 * FF-PIA: demand-driven first-fit RBMSA with GN admission control.
 *
 * Underprovisioned traffic demands are initially ordered by decreasing unmet
 * capacity. Only fiber 0 is considered on every route hop. A working lightpath
 * is placed using a route/band/modulation/slot first-fit search, then mirrored
 * for the reverse direction.
 *
 * A candidate that is infeasible or disturbs committed lightpaths is rejected
 * and the slot window advances by one. If it disturbs only staged lightpaths,
 * each affected symmetric connection is permanently removed, its capacity is
 * restored once per direction, and its forward demand is requeued with an
 * additional requeueMarginStepDb GSNR margin.
 */

BEGIN_ALLOC_FUNCTION(FirstFit)
{

  // ── 0. Allocator configuration ─────────────────────────────────────────────

  const double baseAcceptanceMarginDb = 0.00;
  const double requeueMarginStepDb = 0.3; // Extra margin per rollback/requeue.

  // ── 1. Underprovisioned Demands queue initialization ───────────────────────
  std::deque<std::tuple<int, int, double>> underprovisionedDemands;
  std::map<std::pair<int, int>, int>       demandRequeueCounts;

  // Initialize with all underprovisioned demands.
  FOR_EACH_DEMAND_SYMMETRIC {
    const double remaining = CURRENT_DEMAND.getUnprovisionedCapacity();
    if (remaining > 0) {
      underprovisionedDemands.emplace_back(SRC, DST, remaining);
      demandRequeueCounts[{SRC, DST}] = 0;
    }
  }
  // Serve the demand with the largest currently unmet capacity first.
  std::sort(underprovisionedDemands.begin(), underprovisionedDemands.end(),
            [](auto &a, auto &b) { return std::get<2>(a) > std::get<2>(b); });

  // ── 2. Underprovisioned Demands queue and per-demand placement ─────────────
  while (!underprovisionedDemands.empty()) {

    auto [SRC, DST, _] = underprovisionedDemands.front();
    underprovisionedDemands.pop_front();
    auto &CURRENT_DEMAND  = DEMANDS[SRC][DST];
    double requiredCapacity = CURRENT_DEMAND.getUnprovisionedCapacity();

    // Get the current margin for this demand, based on its requeue count.
    const double candidateMarginDb = baseAcceptanceMarginDb +
        requeueMarginStepDb * demandRequeueCounts[std::make_pair(SRC, DST)];

    // Find the smallest configured bitrate that covers the remaining capacity.
    auto chooseStartingBitRateIdx = [&](double cap) -> int {
      int idx = static_cast<int>(BITRATES.size()) - 1;
      for (int i = 0; i < static_cast<int>(BITRATES.size()); ++i)
        if (BITRATES[i]->getBitRate() >= cap) { idx = i; break; }
      return idx;
    };

    int currentBitRateIdx = chooseStartingBitRateIdx(requiredCapacity);

    while (requiredCapacity > 0 && currentBitRateIdx >= 0) {
        // Was the current LP placed?
        bool currentLightpathPlaced = false;
        // Last working LP's bitrate index:
        int  placedBitRateIdx = -1;

        // Create the connection object to be allocated
        auto conn = CREATE_CONNECTION(SRC, DST);

        // ── 3. Candidate search: bitrate → route → band → modulation → slots
        for (int bitRateIdx = currentBitRateIdx; bitRateIdx >= 0; --bitRateIdx) {
          auto bitRate = BITRATES[bitRateIdx];
          conn->setBitRate(bitRate);

          // Search and admit one working lightpath on one route.
          auto tryPlaceOnRoute = [&](size_t routeIdx) -> bool {
            auto fiberSpec = LINK(SRC, DST, routeIdx, 0)
                                ->getFiber(0).getFiberSpec();

            for (mt::Band band : fiberSpec->getBands()) {
              const double slotWidthHz =
                  fiberSpec->getBandConfig(band).slotWidthHz;
              const int numSlots = LINK(SRC, DST, routeIdx, 0)
                                      ->getFiber(0)
                                      .getNumberOfSlots(0, band, 0);

              // A slot is unavailable if any hop of this route uses it.
              std::vector<bool> representativeSlots(numSlots, false);
              for (size_t linkIdx = 0; linkIdx < NUM_LINKS(SRC, DST, routeIdx); ++linkIdx) {
                const auto &slots = LINK(SRC, DST, routeIdx, linkIdx)->getFiber(0).getSlots(0, band, 0);
                for (int slotIdx = 0; slotIdx < numSlots; ++slotIdx) {
                  representativeSlots[slotIdx] = representativeSlots[slotIdx] || (slots.at(slotIdx) != 0);
                }
              }
              // ── 4. Candidate search: modulation → first-fit slot window ─────────────
              for (int modIdx : VALID_MODULATIONS(routeIdx, bitRateIdx, band, 0.0)) {

                const int requiredSlots = MODULATIONS(bitRateIdx)[modIdx] ->getRequiredSlots(slotWidthHz);

                int currentNumberSlots = 0;
                int currentSlotIdx = 0;
                for (int slotIdx = 0; slotIdx < numSlots; ++slotIdx) {
                  if (representativeSlots[slotIdx]) {
                    currentNumberSlots = 0;
                    currentSlotIdx = slotIdx + 1;
                  } else if (currentNumberSlots < requiredSlots) {
                    ++currentNumberSlots;
                  }

                  if (currentNumberSlots < requiredSlots) continue;

                  auto lp = CREATE_LIGHTPATH(bitRateIdx, modIdx);
                  for (size_t linkIdx = 0;
                      linkIdx < NUM_LINKS(SRC, DST, routeIdx); ++linkIdx) {
                    ADD_LINK_TO_LIGHTPATH(
                        lp,
                        LINK(SRC, DST, routeIdx, linkIdx)->getId(),
                        /*Fiber Idx*/ 0,
                        /*Core Idx*/  0,
                        band,
                        /*Mode Idx*/  0,
                        currentSlotIdx,
                        requiredSlots,
                        conn->getId());
                  }

                  auto lpResult = CONTROLLER.checkFeasibility(
                      *lp, &NEW_CONNECTIONS, candidateMarginDb);

                  // Reject this window if the candidate or a committed LP fails.
                  if (!lpResult.candidateFeasible || !lpResult.committedFeasible) {
                    currentNumberSlots--;
                    currentSlotIdx++;
                    continue;
                  }

                  // Roll back staged pairs made infeasible by this candidate.
                  if (!lpResult.stagedFeasible) {
                    std::set<int> failedIds;
                    for (const auto &[signedId, lightpath] : lpResult.staged) {
                      if (!lightpath.isFeasible(CONTROLLER.getOperationalThresholdMarginDb())) {
                        int canonicalId = std::abs(signedId);
                        auto stagedIt = NEW_CONNECTIONS.find(canonicalId);
                        if (stagedIt == NEW_CONNECTIONS.end() || !stagedIt->second) {
                          throw std::logic_error(
                              "FF_PIA: staged connection was not found.");
                        }
                        if (stagedIt->second->getSrc() > stagedIt->second->getDst()) {
                          canonicalId = stagedIt->second->getMirrorId();
                        }
                        failedIds.insert(canonicalId);
                      }
                    }

                    for (int canonicalId : failedIds) {
                      auto stagedIt = NEW_CONNECTIONS.find(canonicalId);
                      const int mirrorId = stagedIt->second->getMirrorId();
                      auto mirrorIt = NEW_CONNECTIONS.find(mirrorId);
                      if (mirrorIt == NEW_CONNECTIONS.end() || !mirrorIt->second) {
                        throw std::logic_error(
                            "FF_PIA: staged mirror connection not found during rollback.");
                      }

                      const int rolledBackSrc = stagedIt->second->getSrc();
                      const int rolledBackDst = stagedIt->second->getDst();

                      // Release the working path and restore demand capacity.
                      for (int id : {canonicalId, mirrorId}) {
                        const auto &connection = *NEW_CONNECTIONS.at(id);
                        const auto &allocatedLp = connection.getWorkingLightpath();
                        for (size_t hop = 0;
                            hop < allocatedLp.getLinks().size(); ++hop) {
                          auto &fiber =
                              NETWORK.getLink(allocatedLp.getLinks()[hop])
                                  .getFiber(allocatedLp.getFibers()[hop]);
                          for (int slot : allocatedLp.getSlots()[hop]) {
                            const int owner = fiber.getSlot(
                                allocatedLp.getCores()[hop],
                                allocatedLp.getBands()[hop],
                                allocatedLp.getModes()[hop], slot);
                            if (owner == id) {
                              fiber.setSlot(
                                  allocatedLp.getCores()[hop],
                                  allocatedLp.getBands()[hop],
                                  allocatedLp.getModes()[hop], slot, 0);
                            } else if (owner != 0) {
                              throw std::logic_error(
                                  "FF_PIA: staged slot is owned by another connection.");
                            }
                          }
                        }

                        DEMANDS[connection.getSrc()][connection.getDst()]
                            .subtractAllocatedCapacity(
                                connection.getBitRate().getBitRate());
                      }

                      // Requeue only the forward demand and increase its margin.
                      auto &rolledBackDemand =
                          DEMANDS[rolledBackSrc][rolledBackDst];
                      if (!rolledBackDemand.isProvisioned()) {
                        const auto key =
                            std::make_pair(rolledBackSrc, rolledBackDst);
                        const bool alreadyQueued = std::any_of(
                            underprovisionedDemands.begin(),
                            underprovisionedDemands.end(),
                            [&key](const auto &entry) {
                              return std::get<0>(entry) == key.first &&
                                    std::get<1>(entry) == key.second;
                            });
                        if (!alreadyQueued) {
                          underprovisionedDemands.emplace_back(
                              rolledBackSrc, rolledBackDst, 0);
                          demandRequeueCounts[key] += 1;
                        }
                      }

                      // Recompute the physical state affected by each direction.
                      for (int id : {canonicalId, mirrorId}) {
                        CONTROLLER.getGnEngine()->computeAffectedByConnection(
                            NETWORK, CONNECTIONS, NEW_CONNECTIONS,
                            *NEW_CONNECTIONS.at(id));
                      }
                      for (int id : {canonicalId, mirrorId}) {
                        NEW_CONNECTIONS.erase(id);
                      }
                    }
                  }

                  conn->setWorkingLightpath(std::move(lp));
                  return true;
                }
              }
            }
            return false;
          };

          // Working placement is mandatory and uses the first feasible route.
          const size_t routeCount = NUM_ROUTES(SRC, DST);
          bool workingPlaced = false;
          for (size_t routeIdx = 0; routeIdx < routeCount; ++routeIdx) {
            if (tryPlaceOnRoute(routeIdx)) {
              workingPlaced = true;
              break;
            }
          }
          if (!workingPlaced) continue;

          // Mirror the allocation considering we are working with symmetric provisioning.
          auto mirrorConn = CREATE_MIRROR_CONNECTION(conn);
          ALLOCATE_CONNECTION(conn, mirrorConn);
          requiredCapacity -= bitRate->getBitRate();
          placedBitRateIdx = bitRateIdx;
          currentLightpathPlaced = true;
          break;
        } // bitrate

        // No bitrate fits this demand; stop trying to provision it.
        if (!currentLightpathPlaced) break;

        // Cap future bitrate attempts to what just worked.
        currentBitRateIdx = std::min(
            placedBitRateIdx,
            chooseStartingBitRateIdx(requiredCapacity));
    } // demand capacity while
  } // UD queue while
}
END_ALLOC_FUNCTION
