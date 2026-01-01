#ifndef __MY_LEO_SATELLITETOGROUNDLINK_H_
#define __MY_LEO_SATELLITETOGROUNDLINK_H_

#include "omnetpp/cmessage.h"
#include "omnetpp/csimplemodule.h"
#include <omnetpp.h>

using namespace omnetpp;

class SatelliteToGroundLink : public cSimpleModule {
private:
  double distance;
  double dataRate;
  double baseLatency;

  double calculateLatency(double dist) const;

protected:
  virtual void initialize() override;
  virtual void handleMessage(cMessage *msg) override;
  virtual void finish() override;
};
#endif