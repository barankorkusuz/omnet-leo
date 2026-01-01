#ifndef __MY_LEO_SATELLITE_H
#define __MY_LEO_SATELLITE_H

#include "omnetpp/cmessage.h"
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

  void findNeighborSatellites();
  void updateNeighborList();
  double calculateDistanceToSatellite(cModule *otherSatellite) const;

  void sendToNeighbor(cModule *targetsatellite, cMessage *msg);

protected:
  virtual void initialize() override;
  virtual void handleMessage(cMessage *msg) override;
  virtual void finish() override;
};

#endif