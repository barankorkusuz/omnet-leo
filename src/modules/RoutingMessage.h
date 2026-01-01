#ifndef __MY_LEO_ROUTINGMESSAGE_H_
#define __MY_LEO_ROUTINGMESSAGE_H_

#include "omnetpp/cmessage.h"
#include <omnetpp.h>
#include <vector>

using namespace omnetpp;

class RoutingMessage : public cMessage {

public:
  int sourceId;
  std::vector<int> destIds;
  std::vector<double> costs;

  RoutingMessage(const char *name = nullptr) : cMessage(name) {}
  RoutingMessage(const RoutingMessage &other) : cMessage(other) {
    sourceId = other.sourceId;
    destIds = other.destIds;
    costs = other.costs;
  }

  virtual RoutingMessage *dup() const override {
    return new RoutingMessage(*this);
  }
};

#endif