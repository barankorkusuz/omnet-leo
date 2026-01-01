#include "SatelliteToGroundLink.h"
#include "omnetpp/cmessage.h"
#include <cmath>

Define_Module(SatelliteToGroundLink);

void SatelliteToGroundLink::initialize() {

  distance = par("distance");
  dataRate = par("dataRate");
  baseLatency = par("baseLatency");

  EV << "Satellite-to-Ground Link initialized: distance=" << distance
     << " km, dataRate=" << dataRate << " Gbps, baseLatency=" << baseLatency
     << " ms" << endl;
}

void SatelliteToGroundLink::handleMessage(cMessage *msg) {
  double latency = calculateLatency(distance);

  sendDelayed(msg, latency, "linkOut");
}

double SatelliteToGroundLink::calculateLatency(double dist) const {
  const double SPEED_OF_LIGHT = 299792.458;
  double propogationDelay = dist / SPEED_OF_LIGHT;

  double totalLatency = propogationDelay + (baseLatency / 1000.0);

  return totalLatency;
}

void SatelliteToGroundLink::finish() {
  EV << "Satellite-to-Ground Link finished" << endl;
}