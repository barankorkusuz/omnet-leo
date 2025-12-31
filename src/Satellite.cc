#include "Satellite.h"

void Satellite::initialize() { EV << "Satellite initialized" << endl; }

void Satellite::handleMessage(cMessage *msg) {
  EV << "Satellite received message" << endl;
  delete msg;
}

void Satellite::finish() { EV << "Satellite finished" << endl; }

Define_Module(Satellite);