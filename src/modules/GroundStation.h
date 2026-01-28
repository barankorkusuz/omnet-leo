#ifndef __MY_LEO_GROUNDSTATION_H_
#define __MY_LEO_GROUNDSTATION_H_

#include "../utils/PositionUtils.h"
#include "omnetpp/cmessage.h"
#include "omnetpp/cmodule.h"
#include "omnetpp/cqueue.h"
#include <omnetpp.h>

using namespace omnetpp;

class GroundStation : public cSimpleModule {

private:
  int myAddress;
  Position3D position;
  double maxRange;
  cModule *currentSatellite;     // current connected satellite
  int currentSatGateIndex;       // gate index on satellite side for this GS
  cMessage *handoverTimer;
  cMessage *trafficTimer;

  // Dynamic connection management
  void connectToSatellite(cModule *satellite);
  void disconnectFromSatellite();

  // Queue Management
  cQueue *txQueue;
  cMessage *txFinishTimer;
  int maxQueueSize;
  void sendOrQueue(cMessage *msg, const char *gateName, int gateIndex);
  void processTxQueue();

  cOutVector *endToEndDelay;
  long packetsSent;
  long packetsReceived;
  long packetsDropped;
  long totalBitsReceived;

  simtime_t firstPacketTime;
  simtime_t lastPacketTime;

  cModule *findNearestSatellite() const;

  void performHandover();
  void sendToCurrentSatellite(cMessage *msg);

protected:
  virtual void initialize() override;
  virtual void handleMessage(cMessage *msg) override;
  virtual void finish() override;
};

#endif