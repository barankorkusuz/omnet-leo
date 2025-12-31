#ifndef __MY_LEO_SATELLITE_H
#define __MY_LEO_SATELLITE_H

#include <omnetpp.h>

using namespace omnetpp;

class Satellite : public cSimpleModule {
protected:
  virtual void initialize() override;
  virtual void handleMessage(cMessage *msg) override;
  virtual void finish() override;
};

#endif