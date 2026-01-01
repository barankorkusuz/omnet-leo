#include "Satellite.h"
#include "omnetpp/cmessage.h"
#include "omnetpp/cmodule.h"
#include <cstring>

void Satellite::initialize() {

  satelliteId = par("satelliteId");

  orbitParams.altitude = par("altitude");
  orbitParams.inclination = par("inclination");
  orbitParams.period = par("period");
  orbitParams.initialAngle = par("initialAngle");

  maxISLRange = par("maxISLRange");

  currentPosition = updateLEOOrbit(orbitParams, 0.0);
  updateNeighborList();
  findNeighborSatellites();

  if (satelliteId == 2 && !neighbors.empty()) {
    cMessage *testMsg = new cMessage("Test from sat2 to sat3");
    sendToNeighbor(neighbors[0].module, testMsg);
  }

  EV << "Satellite " << satelliteId << " initial position: ("
     << currentPosition.x << ", " << currentPosition.y << ", "
     << currentPosition.z << ") km" << endl;

  updateTimer = new cMessage("updatePosition");
  scheduleAt(simTime() + 1.0, updateTimer);
}

void Satellite::handleMessage(cMessage *msg) {
  if (msg == updateTimer) {

    double simTimeSeconds = simTime().dbl();

    currentPosition = updateLEOOrbit(orbitParams, simTimeSeconds);

    getDisplayString().setTagArg("p", 0, currentPosition.x);
    getDisplayString().setTagArg("p", 1, currentPosition.y);

    updateNeighborList();
    findNeighborSatellites();

    EV << "Satellite " << satelliteId << " position: (" << currentPosition.x
       << ", " << currentPosition.y << ", " << currentPosition.z << ") km"
       << endl;

    scheduleAt(simTime() + 1.0, updateTimer);
  } else {
    delete msg;
  }
}

void Satellite::finish() {
  if (updateTimer) {
    cancelAndDelete(updateTimer);
  }
}

void Satellite::findNeighborSatellites() {
  // print neighbors.
  for (const auto &neighbor : neighbors) {
    EV << "Neighbours of " << satelliteId << neighbor.module->par("satelliteId")
       << endl;
  }
}

double Satellite::calculateDistanceToSatellite(cModule *otherSatellite) const {
  Position3D myPos = currentPosition;

  OrbitParams otherParams;
  otherParams.altitude = otherSatellite->par("altitude");
  otherParams.inclination = otherSatellite->par("inclination");
  otherParams.period = otherSatellite->par("period");
  otherParams.initialAngle = otherSatellite->par("initialAngle");

  Position3D otherPos = updateLEOOrbit(otherParams, simTime().dbl());

  return calculateDistance(myPos, otherPos);
}

void Satellite::updateNeighborList() {
  neighbors.clear();

  cModule *network = getParentModule();
  if (!network) {
    EV << "ERROR: Network not found" << endl;
    return;
  }

  int gateIndex = 0;

  for (cModule::SubmoduleIterator it(network); !it.end(); ++it) {
    cModule *submod = *it;

    if (submod == this || strcmp(submod->getClassName(), "Satellite") != 0) {
      continue;
    }
    double distance = calculateDistanceToSatellite(submod);

    if (distance <= maxISLRange) {
      NeighborInfo neighbor;
      neighbor.module = submod;
      neighbor.distance = distance;
      neighbor.gateIndex = gateIndex;

      neighbors.push_back(neighbor);

      EV << "Satellite " << satelliteId << " connected to "
         << submod->par("satelliteId") << " at distance: " << distance << " km"
         << endl;

      gateIndex++;
    }
  }
}

void Satellite::sendToNeighbor(cModule *targetSatellite, cMessage *msg) {

  for (const auto &neighbor : neighbors) {
    if (neighbor.module == targetSatellite) {
      if (gateSize("radioOut$o") > neighbor.gateIndex) {
        send(msg, "radioOut$o", neighbor.gateIndex);
        return;
      }
    }
  }
  EV << "ERROR: Target satellite not in neighbor list! Sender: " << satelliteId
     << " Target: " << targetSatellite->par("satelliteId") << endl;
  delete msg;
}

Define_Module(Satellite);