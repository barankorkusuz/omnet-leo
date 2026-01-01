#ifndef __MY_LEO_DATAPACKET_H_
#define __MY_LEO_DATAPACKET_H_

#include "omnetpp/cpacket.h"
#include "omnetpp/simtime.h"
#include <omnetpp.h>
#include <string>

using namespace omnetpp;

class DataPacket : public cPacket {
public:
  int sourceId;
  int destinationId;
  int packetId;
  int hopCount;
  std::string payload;

  simtime_t creationTime;

  DataPacket(const char *name = nullptr) : cPacket(name) {
    sourceId = -1;
    destinationId = -1;
    packetId = 0;
    hopCount = 0;
    creationTime = simTime();
    setBitLength(1024 * 8); // Varsayılan boyut: 1KB
  }
  DataPacket(const DataPacket &other) : cPacket(other) {
    sourceId = other.sourceId;
    destinationId = other.destinationId;
    packetId = other.packetId;
    hopCount = other.hopCount;
    payload = other.payload;
    creationTime = other.creationTime;
  }
  virtual DataPacket *dup() const override { return new DataPacket(*this); }
};
#endif