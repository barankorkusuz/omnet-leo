#ifndef __MY_LEO_DATAPACKET_H_
#define __MY_LEO_DATAPACKET_H_

#include "omnetpp/cmessage.h"
#include <omnetpp.h>
#include <string>

using namespace omnetpp;

class DataPacket : public cMessage {
public:
  int sourceId;
  int destinationId;
  int packetId;
  int hopCount;
  std::string payload;

  DataPacket(const char *name = nullptr) : cMessage(name) {
    sourceId = -1;
    destinationId = -1;
    packetId = 0;
    hopCount = 0;
  }
  DataPacket(const DataPacket &other) : cMessage(other) {
    sourceId = other.sourceId;
    destinationId = other.destinationId;
    packetId = other.packetId;
    hopCount = other.hopCount;
    payload = other.payload;
  }
  virtual DataPacket *dup() const override { return new DataPacket(*this); }
};
#endif