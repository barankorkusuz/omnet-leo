#ifndef __MY_LEO_INTERSATELLITELINK_H_
#define __MY_LEO_INTERSATELLITELINK_H_

#include "omnetpp/csimplemodule.h"
#include "omnetpp/resultfilters.h"
#include <omnetpp.h>

using namespace omnetpp;

class InterSatelliteLink : public cSimpleModule {

private:
  double distance;
  double dataRate;
  double baseLatency;

  // calculates latency (s) based on distance
  double calculateLatency(double dist) const;

protected:
  virtual void initialize() override;
  virtual void handleMessage(cMessage *msg) override;
  virtual void finish() override;
};

#endif