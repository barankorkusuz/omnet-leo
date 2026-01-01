#include "GroundStation.h"
#include "DataPacket.h"
#include "omnetpp/checkandcast.h"
#include "omnetpp/cmessage.h"
#include "omnetpp/cmodule.h"
#include "omnetpp/csimulation.h"
#include <cstring>

Define_Module(GroundStation);

void GroundStation::initialize() {

  position.x = par("x");
  position.y = par("y");
  position.z = par("z");

  maxRange = par("maxRange");
  currentSatellite = nullptr;

  EV << "GroundStation initialized at position: (" << position.x << ", "
     << position.y << ", " << position.z << ") km" << endl;

  handoverTimer = new cMessage("handoverTimer");
  scheduleAt(simTime() + 5.0, handoverTimer);

  // perform to find first satellite to connect
  performHandover();

  if (currentSatellite) {
    DataPacket *packet = new DataPacket("DataPacket");
    packet->sourceId = 0;      // Ground Station ID
    packet->destinationId = 3; // sat3
    packet->packetId = 1;
    packet->hopCount = 0;
    packet->payload = "Hello from Ground Station";

    sendToCurrentSatellite(packet);
    EV << "GroundStation sent DataPacket to Satellite "
       << currentSatellite->par("satelliteId").intValue()
       << " (destination: " << packet->destinationId << ")" << endl;
  }
}

void GroundStation::handleMessage(cMessage *msg) {
  // TODO: implement

  if (msg == handoverTimer) { // handover timer
    performHandover();
    scheduleAt(simTime() + 5.0, handoverTimer);
  } else if (dynamic_cast<DataPacket *>(msg) != nullptr) {
    // DataPacket alındı
    DataPacket *packet = check_and_cast<DataPacket *>(msg);
    EV << "GroundStation received DataPacket #" << packet->packetId << " from "
       << packet->sourceId << " (hops: " << packet->hopCount << ")" << endl;
    delete packet;
  } else { // normal messages
    EV << "GroundStation received message: " << msg->getName() << endl;
    delete msg;
  }
}

void GroundStation::finish() {
  if (handoverTimer) {
    cancelAndDelete(handoverTimer);
  }

  EV << "GroundStation module finish" << endl;
}

cModule *GroundStation::findNearestSatellite() const {

  cModule *network = getParentModule();
  if (!network) {
    EV << "ERROR: GroundStation couldnt find network!" << endl;
    return nullptr;
  }

  cModule *nearestSat = nullptr;
  double minDistance = maxRange;

  for (cModule::SubmoduleIterator it(network); !it.end(); ++it) {
    cModule *submod = *it;

    if (strcmp(submod->getClassName(), "Satellite") != 0) {
      continue;
    }

    OrbitParams orbitParams;
    orbitParams.altitude = submod->par("altitude");
    orbitParams.inclination = submod->par("inclination");
    orbitParams.period = submod->par("period");
    orbitParams.initialAngle = submod->par("initialAngle");

    Position3D satPos = updateLEOOrbit(orbitParams, simTime().dbl());
    double distance = calculateDistance(position, satPos);

    if (distance <= maxRange && distance < minDistance) {
      minDistance = distance;
      nearestSat = submod;
    }
  }
  return nearestSat;
}

void GroundStation::performHandover() {
  cModule *nearestSat = findNearestSatellite();

  if (nearestSat != currentSatellite) {
    if (currentSatellite) {
      EV << "GroundStation handover from Satellite "
         << currentSatellite->par("satelliteId").intValue() << endl;
    }
    currentSatellite = nearestSat;

    if (currentSatellite) {
      EV << "GroundStation connected to Satellite "
         << currentSatellite->par("satelliteId").intValue() << endl;
    } else {
      EV << "GroundStation: No satellite in range!" << endl;
    }
  }
}
void GroundStation::sendToCurrentSatellite(cMessage *msg) {
  if (!currentSatellite) {
    EV << "ERROR: No satellite connected!" << endl;
    delete msg;
    return;
  }
  int gate = getGateIndexForSatellite(currentSatellite);
  send(msg, "groundLink$o", gate);
}

int GroundStation::getGateIndexForSatellite(cModule *satellite) const {
  int satId = satellite->par("satelliteId").intValue();
  return satId - 1;
}