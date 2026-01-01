#ifndef __MY_LEO_GROUNDSTATION_H_
#define __MY_LEO_GROUNDSTATION_H_

#include "../utils/PositionUtils.h"
#include "omnetpp/cmessage.h"
#include "omnetpp/cmodule.h"
#include <omnetpp.h>

using namespace omnetpp;

class GroundStation : public cSimpleModule {

private:
  Position3D position;
  double maxRange;
  cModule *currentSatellite; // current connected satellite
  cMessage *handoverTimer;

  cOutVector *endToEndDelay;
  long packetsSent;
  long packetsReceived;

  cModule *findNearestSatellite() const;

  void performHandover();
  void sendToCurrentSatellite(cMessage *msg);
  int getGateIndexForSatellite(cModule *satellite) const;

protected:
  virtual void initialize() override;
  virtual void handleMessage(cMessage *msg) override;
  virtual void finish() override;
};

#endif