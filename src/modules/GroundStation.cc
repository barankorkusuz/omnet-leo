#include "GroundStation.h"
#include "DataPacket.h"
#include "omnetpp/checkandcast.h"
#include "omnetpp/cmessage.h"
#include "omnetpp/cmodule.h"
#include "omnetpp/csimulation.h"
#include <cstring>

Define_Module(GroundStation);

void GroundStation::initialize() {

  GeoCoord geo;
  geo.latitude = par("latitude");
  geo.longitude = par("longitude");
  geo.altitude = par("altitude");

  position = geoToECEF(geo);

  // Set initial 2D Map position
  Position3D screenPos = geoToScreen(geo, 1000.0, 500.0);
  getDisplayString().setTagArg("p", 0, (long)screenPos.x);
  getDisplayString().setTagArg("p", 1, (long)screenPos.y);

  maxRange = par("maxRange");
  currentSatellite = nullptr;

  endToEndDelay = new cOutVector("endToEndDelay");
  packetsSent = 0;
  packetsReceived = 0;

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

  if (msg == handoverTimer) { // handover timer
    performHandover();
    scheduleAt(simTime() + 5.0, handoverTimer);
  } else if (dynamic_cast<DataPacket *>(msg) != nullptr) {
    // DataPacket received
    DataPacket *packet = check_and_cast<DataPacket *>(msg);
    packetsReceived++;

    // End-to-end delay
    simtime_t delay = simTime() - packet->creationTime;
    endToEndDelay->record(delay.dbl());

    EV << "GroundStation received DataPacket #" << packet->packetId << " from "
       << packet->sourceId << " (hops: " << packet->hopCount
       << ", delay: " << delay << "s)" << endl;
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

  // statistics
  EV << "=== GroundStation Statistics ===" << endl;
  EV << "Packets Sent: " << packetsSent << endl;
  EV << "Packets Received: " << packetsReceived << endl;
  EV << "Total Packets: " << (packetsSent + packetsReceived) << endl;

  delete endToEndDelay;

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
    orbitParams.semiMajorAxis = EARTH_RADIUS + submod->par("altitude").doubleValue();
    orbitParams.inclination = submod->par("inclination");
    orbitParams.raan = submod->par("raan");
    orbitParams.argPerigee = submod->par("argPerigee");
    orbitParams.trueAnomaly = submod->par("initialAngle");
    orbitParams.eccentricity = submod->par("eccentricity");

    Position3D satPos = calculateSatellitePositionECEF(orbitParams, simTime().dbl());
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
  if (dynamic_cast<DataPacket *>(msg) != nullptr) {
    packetsSent++;
  }

  int gate = getGateIndexForSatellite(currentSatellite);
  send(msg, "groundLink$o", gate);
}

int GroundStation::getGateIndexForSatellite(cModule *satellite) const {
  int numGates = gateSize("groundLink$o");
  for (int i = 0; i < numGates; i++) {
      const cGate *outGate = gate("groundLink$o", i);
      if (outGate->isConnected() && outGate->getPathEndGate()->getOwnerModule() == satellite) {
          return i;
      }
  }
  return -1; // Not found
}