#include "InterSatelliteLink.h"
#include "omnetpp/cmessage.h"

Define_Module(InterSatelliteLink);

void InterSatelliteLink::initialize() {

  distance = par("distance");
  dataRate = par("dataRate");
  baseLatency = par("baseLatency");

  EV << "ISL initialized: distance=" << distance << " km, dataRate=" << dataRate
     << " Gbps, baseLatency=" << baseLatency << " ms" << endl;
}

void InterSatelliteLink::handleMessage(cMessage *msg) {

  double latency = calculateLatency(distance);

  sendDelayed(msg, latency, "linkOut");
}

double InterSatelliteLink::calculateLatency(double dist) const {

  const double SPEED_OF_LIGHT = 299792.456;
  double propogationDelay = dist / SPEED_OF_LIGHT;
  double totalLatency = propogationDelay + (baseLatency / 1000.0);

  return totalLatency;
}

void InterSatelliteLink::finish() { EV << "ISL finished" << endl; }