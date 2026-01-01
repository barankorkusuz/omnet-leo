#include "Satellite.h"
#include "modules/DataPacket.h"
#include "modules/RoutingMessage.h"
#include "omnetpp/checkandcast.h"
#include "omnetpp/cmessage.h"
#include "omnetpp/cmodule.h"
#include <cstring>

void Satellite::initialize() {

  satelliteId = par("satelliteId").intValue();

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
  } else if (dynamic_cast<RoutingMessage *>(msg) != nullptr) {
    processRoutingMessage(check_and_cast<RoutingMessage *>(msg));
  } else if (dynamic_cast<DataPacket *>(msg) != nullptr) {
    DataPacket *packet = check_and_cast<DataPacket *>(msg);
    // I am the receiver take the message
    if (packet->destinationId == satelliteId) {
      EV << "Satellite " << satelliteId << " received packet #"
         << packet->packetId << " from " << packet->sourceId
         << " (hops: " << packet->hopCount << ")" << endl;
      delete packet;
    }
    // forward
    else {
      packet->hopCount++;
      routeMessage(packet, packet->destinationId);
    }
  }

  else {
    // Gelen mesajları işle
    EV << "Satellite " << satelliteId << " received message: " << msg->getName()
       << endl;
    if (strcmp(msg->getName(), "TestFromGS") != 0) {
      cMessage *reply = new cMessage("ReplyFromSat");
      send(reply, "radioOut$o", 2);
    }
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
    EV << "Neighbours of " << satelliteId
       << neighbor.module->par("satelliteId").intValue() << endl;
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
         << submod->par("satelliteId").intValue()
         << " at distance: " << distance << " km" << endl;

      gateIndex++;
    }
  }
  updateRoutingTable();
  broadcastRoutingTable();
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
     << " Target: " << targetSatellite->par("satelliteId").intValue() << endl;
  delete msg;
}

void Satellite::updateRoutingTable() {
  routingTable.clear();

  for (const auto &neighbor : neighbors) {
    RoutingEntry entry;
    entry.destinationId = neighbor.module->par("satelliteId").intValue();
    entry.nextHopId = entry.destinationId;
    entry.cost = neighbor.distance;

    routingTable.push_back(entry);
  }
  EV << "Satellite " << satelliteId << " routing table updated with "
     << routingTable.size() << " entries" << endl;
}
void Satellite::routeMessage(cMessage *msg, int destinationId) {
  for (const auto &entry : routingTable) {
    if (entry.destinationId == destinationId) {
      for (const auto &neighbor : neighbors) {
        if (neighbor.module->par("satelliteId").intValue() == entry.nextHopId) {
          sendToNeighbor(neighbor.module, msg);
          EV << "Satellite " << satelliteId << " routing message to "
             << destinationId << " via " << entry.nextHopId << endl;
          return;
        }
      }
    }
  }
}

void Satellite::broadcastRoutingTable() {
  RoutingMessage *rmsg = new RoutingMessage("RoutingUpdate");
  rmsg->sourceId = satelliteId;

  for (const auto &entry : routingTable) {
    rmsg->destIds.push_back(entry.destinationId);
    rmsg->costs.push_back(entry.cost);
  }
  rmsg->destIds.push_back(satelliteId);
  rmsg->costs.push_back(0.0);

  for (const auto &neighbor : neighbors) {

    RoutingMessage *msgCopy = rmsg->dup();
    sendToNeighbor(neighbor.module, msgCopy);
  }

  delete rmsg;
  EV << "Satellite " << satelliteId << " broadcasted routing table" << endl;
}
void Satellite::processRoutingMessage(RoutingMessage *msg) {
  bool updated = false;

  for (size_t i = 0; i < msg->destIds.size(); i++) {
    int destId = msg->destIds[i];
    double receivedCost = msg->costs[i];

    double linkCost = 0.0;
    for (const auto &neighbor : neighbors) {
      if (neighbor.module->par("satelliteId").intValue() == msg->sourceId) {
        linkCost = neighbor.distance;
        break;
      }
    }

    double totalCost = receivedCost + linkCost;

    bool found = false;
    for (auto &entry : routingTable) {
      if (entry.destinationId == destId) {
        if (totalCost < entry.cost) {
          entry.nextHopId = msg->sourceId;
          entry.cost = totalCost;
          updated = true;
        }
        found = true;
        break;
      }
    }
    if (!found && destId != satelliteId) {
      RoutingEntry entry;
      entry.destinationId = destId;
      entry.nextHopId = msg->sourceId;
      entry.cost = totalCost;
      routingTable.push_back(entry);
      updated = true;
    }
  }
  if (updated) {
    EV << "Satellite " << satelliteId << " updated routing table from "
       << msg->sourceId << endl;
  }
  delete msg;
}
Define_Module(Satellite);