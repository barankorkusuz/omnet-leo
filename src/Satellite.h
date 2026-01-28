#ifndef __MY_LEO_SATELLITE_H
#define __MY_LEO_SATELLITE_H

#include "modules/RoutingMessage.h"
#include "omnetpp/chistogram.h"
#include "omnetpp/cmessage.h"
#include "omnetpp/coutvector.h"
#include "omnetpp/cpacketqueue.h"
#include "utils/PositionUtils.h"
#include <omnetpp.h>
#include <vector>

using namespace omnetpp;

#include "omnetpp/cqueue.h"

// ... existing includes ...

class Satellite : public cSimpleModule {

private:
  int satelliteId;
  // ... existing params ...

  // Queue Management
  cQueue *txQueue;
  cMessage *txFinishTimer;
  int maxQueueSize; // Limit queue size to simulate drop
  
  // Helper to handle sending
  void sendOrQueue(cMessage *msg, const char *gateName, int gateIndex);
  void processTxQueue();

  OrbitParams orbitParams;
  // ... existing members ...
  Position3D currentPosition;
  cMessage *updateTimer;
  cMessage *trafficTimer;

  double maxISLRange;

  struct NeighborInfo {
    cModule *module;
    double distance;
    int gateIndex;
  };
  std::vector<NeighborInfo> neighbors;

  struct RoutingEntry {
    int destinationId;
    int nextHopId;
    double cost;
  };
  std::vector<RoutingEntry> routingTable;
  void updateRoutingTable();
  void routeMessage(cMessage *msg, int destinationId);

  cOutVector *endToEndDelay;
  cOutVector *hopCountVector;
  cHistogram *hopCountHist;
  long packetsReceived;      // Packets where this satellite was the destination (should be 0)
  long packetsForwarded;     // Packets successfully routed to next hop
  long packetsDropped;       // Packets dropped (no route or queue full)
  long totalBitsForwarded;   // Total bits forwarded (for throughput calculation)

  simtime_t firstPacketTime;
  simtime_t lastPacketTime;

  void findNeighborSatellites();
  void updateNeighborList();
  double calculateDistanceToSatellite(cModule *otherSatellite) const;

  void sendToNeighbor(cModule *targetsatellite, cMessage *msg);

  void broadcastRoutingTable();
  void processRoutingMessage(RoutingMessage *msg);

protected:
  virtual void initialize() override;
  virtual void handleMessage(cMessage *msg) override;
  virtual void finish() override;
};

#endif