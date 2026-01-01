#ifndef __MY_LEO_SATELLITE_H
#define __MY_LEO_SATELLITE_H

#include "modules/RoutingMessage.h"
#include "omnetpp/chistogram.h"
#include "omnetpp/cmessage.h"
#include "omnetpp/coutvector.h"
#include "utils/PositionUtils.h"
#include <omnetpp.h>
#include <vector>

using namespace omnetpp;

class Satellite : public cSimpleModule {

private:
  int satelliteId;
  OrbitParams orbitParams;
  Position3D currentPosition;
  cMessage *updateTimer;

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
  long packetsReceived;
  long packetsForwarded;
  long packetsDropped;
  long totalBitsReceived;

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