#include "Satellite.h"
#include "modules/DataPacket.h"
#include "modules/RoutingMessage.h"
#include "omnetpp/checkandcast.h"
#include "omnetpp/chistogram.h"
#include "omnetpp/cmessage.h"
#include "omnetpp/cmodule.h"
#include "omnetpp/coutvector.h"
#include "omnetpp/csimulation.h"
#include <cstring>

void Satellite::initialize() {

  // Queue Init - MUST BE FIRST because updateNeighborList sends packets!
  txQueue = new cQueue("txQueue");
  txFinishTimer = new cMessage("txFinishTimer");
  maxQueueSize = 1000; // Standard Router Buffer size

  satelliteId = par("satelliteId").intValue();

  orbitParams.semiMajorAxis = EARTH_RADIUS + par("altitude").doubleValue();
  orbitParams.inclination = par("inclination");
  orbitParams.raan = par("raan");
  orbitParams.argPerigee = par("argPerigee");
  orbitParams.trueAnomaly = par("initialAngle");
  orbitParams.eccentricity = par("eccentricity");

  maxISLRange = par("maxISLRange");

  currentPosition = calculateSatellitePositionECEF(orbitParams, 0.0);
  updateNeighborList();
  findNeighborSatellites();

  EV << "Satellite " << satelliteId << " initial position: ("
     << currentPosition.x << ", " << currentPosition.y << ", "
     << currentPosition.z << ") km" << endl;

  updateTimer = new cMessage("updatePosition");
  scheduleAt(simTime() + 1.0, updateTimer);

  // Traffic generation disabled - satellites are only routers
  trafficTimer = nullptr;

  endToEndDelay = new cOutVector("endToEndDelay");
  hopCountVector = new cOutVector("hopCount");
  hopCountHist = new cHistogram("hopCountHist");

  packetsReceived = 0;
  packetsForwarded = 0;
  packetsDropped = 0;
  totalBitsForwarded = 0;
  firstPacketTime = 0;
  lastPacketTime = 0;

  EV << "Satellite " << satelliteId << " initialized as ROUTER" << endl;
}

void Satellite::handleMessage(cMessage *msg) {
  if (msg == txFinishTimer) {
    processTxQueue();
  } else if (msg == updateTimer) {

    double simTimeSeconds = simTime().dbl();

    currentPosition =
        calculateSatellitePositionECEF(orbitParams, simTimeSeconds);

    // Update 2D Map Position (Mission Control View)
    GeoCoord geo = ecefToGeo(currentPosition);
    Position3D screenPos = geoToScreen(geo, 1000.0, 500.0);

    getDisplayString().setTagArg("p", 0, (long)screenPos.x);
    getDisplayString().setTagArg("p", 1, (long)screenPos.y);
    // No Z needed for 2D map

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

    hopCountVector->record(packet->hopCount);
    hopCountHist->collect(packet->hopCount);

    // Satellites should NOT be destinations - they are only routers
    if (packet->destinationId == satelliteId) {
      EV << "WARNING: Satellite " << satelliteId << " received packet meant for itself. "
         << "This should not happen - satellites are routers only!" << endl;
      packetsReceived++;  // Count but this should stay 0
      delete packet;
    }
    // forward (satellites are routers - this is the main path)
    else {
      packet->hopCount++;

      bool routeFound = false;
      for (const auto &entry : routingTable) {
        if (entry.destinationId == packet->destinationId) {
          routeFound = true;
          break;
        }
      }
      if (routeFound) {
        packetsForwarded++;
        totalBitsForwarded += packet->getBitLength();

        if (packetsForwarded == 1) {
            firstPacketTime = simTime();
        }
        lastPacketTime = simTime();

        routeMessage(packet, packet->destinationId);
        EV << "Satellite " << satelliteId << " forwarding packet #"
           << packet->packetId << " to " << packet->destinationId
           << " (hops: " << packet->hopCount << ")" << endl;
      } else {
        packetsDropped++;
        EV << "ERROR: Satellite " << satelliteId << " dropped packet #"
           << packet->packetId << " (no route to " << packet->destinationId
           << ")" << endl;
        delete packet;
      }
    }
  } else {
    // Process incoming messages
    EV << "Satellite " << satelliteId << " received message: " << msg->getName()
       << endl;
    if (strcmp(msg->getName(), "TestFromGS") != 0) {
      cMessage *reply = new cMessage("ReplyFromSat");
      sendOrQueue(reply, "radioOut$o", 2);
    }
    delete msg;
  }
}

// --- Queue Logic ---
void Satellite::sendOrQueue(cMessage *msg, const char *gateName,
                            int gateIndex) {
  // Check if gate is valid and connected before queueing
  if (gateIndex < 0 || gateIndex >= gateSize("radioOut$o")) {
    EV << "Satellite " << satelliteId << " dropping packet - invalid gate " << gateIndex << endl;
    packetsDropped++;
    delete msg;
    return;
  }

  cGate *outGate = gate("radioOut$o", gateIndex);
  if (!outGate->isConnected()) {
    EV << "Satellite " << satelliteId << " dropping packet - gate " << gateIndex << " not connected" << endl;
    packetsDropped++;
    delete msg;
    return;
  }

  if (txQueue->getLength() >= maxQueueSize) {
    EV << "Tx Queue Full! Dropping packet " << msg->getName() << endl;
    packetsDropped++;
    delete msg;
    return;
  }

  msg->setContextPointer((void *)(intptr_t)gateIndex);
  txQueue->insert(msg);
  processTxQueue();
}

void Satellite::processTxQueue() {
  if (txQueue->isEmpty())
    return;

  cMessage *msg = (cMessage *)txQueue->front();
  int gateIndex = (int)(intptr_t)msg->getContextPointer();

  // Check if gate index is valid
  if (gateIndex < 0 || gateIndex >= gateSize("radioOut$o")) {
    EV << "Satellite " << satelliteId << " dropping packet - invalid gate index " << gateIndex << endl;
    txQueue->pop();
    packetsDropped++;
    delete msg;
    return;
  }

  cGate *outGate = gate("radioOut$o", gateIndex);

  // Check if gate is connected (dynamic links may have been disconnected)
  if (!outGate->isConnected()) {
    EV << "Satellite " << satelliteId << " dropping packet - gate " << gateIndex << " disconnected (handover)" << endl;
    txQueue->pop();
    packetsDropped++;
    delete msg;
    return;
  }

  cChannel *chan = outGate->getTransmissionChannel();

  if (chan && chan->isBusy()) {
    // Channel busy. Wait until it finishes.
    simtime_t finishTime = chan->getTransmissionFinishTime();

    if (!txFinishTimer->isScheduled()) {
      scheduleAt(finishTime, txFinishTimer);
    }
  } else {
    // Channel free! Send it.
    txQueue->pop();
    send(msg, "radioOut$o", gateIndex);
  }
}

void Satellite::finish() {
  if (updateTimer) {
    cancelAndDelete(updateTimer);
  }
  if (txFinishTimer) {
    cancelAndDelete(txFinishTimer);
  }
  delete txQueue;

  // === ROUTER STATISTICS ===
  EV << "=== Satellite " << satelliteId << " (ROUTER) Statistics ===" << endl;
  EV << "Packets Forwarded: " << packetsForwarded << endl;
  EV << "Packets Dropped: " << packetsDropped << endl;

  long totalPackets = packetsForwarded + packetsDropped;
  EV << "Total Packets Handled: " << totalPackets << endl;

  // --- Router Throughput ---
  double simDuration = simTime().dbl();
  double activeDuration = (lastPacketTime - firstPacketTime).dbl();
  if (activeDuration <= 0.001) {
      activeDuration = simDuration;
  }

  double forwardThroughputBps =
      (activeDuration > 0) ? (double)totalBitsForwarded / activeDuration : 0.0;

  // --- Forward Success Rate ---
  // How many packets were successfully forwarded vs dropped
  double forwardSuccessRate = (totalPackets > 0)
      ? (double)packetsForwarded / totalPackets
      : 1.0;

  EV << "Forward Throughput: " << forwardThroughputBps / 1000000.0 << " Mbps" << endl;
  EV << "Forward Success Rate: " << forwardSuccessRate * 100.0 << " %" << endl;

  // Record scalars for OMNeT++ Analysis
  recordScalar("ForwardThroughput_bps", forwardThroughputBps);
  recordScalar("ForwardSuccessRate", forwardSuccessRate);
  recordScalar("PacketsForwarded", packetsForwarded);
  recordScalar("PacketsDropped", packetsDropped);

  if (hopCountHist->getCount() > 0) {
    EV << "Hop Count - Mean: " << hopCountHist->getMean()
       << ", Min: " << hopCountHist->getMin()
       << ", Max: " << hopCountHist->getMax() << endl;
  }

  delete endToEndDelay;
  delete hopCountHist;
  delete hopCountVector;
}

void Satellite::findNeighborSatellites() {
  // print neighbors.
  for (const auto &neighbor : neighbors) {
    int neighborId = -1;
    if (strcmp(neighbor.module->getClassName(), "Satellite") == 0) {
      neighborId = neighbor.module->par("satelliteId").intValue();
    } else {
      neighborId = neighbor.module->par("address").intValue();
    }
    EV << "Neighbours of " << satelliteId << ": " << neighborId << endl;
  }
}

double Satellite::calculateDistanceToSatellite(cModule *otherSatellite) const {
  Position3D myPos = currentPosition;

  OrbitParams otherParams;
  otherParams.semiMajorAxis =
      EARTH_RADIUS + otherSatellite->par("altitude").doubleValue();
  otherParams.inclination = otherSatellite->par("inclination");
  otherParams.raan = otherSatellite->par("raan");
  otherParams.argPerigee = otherSatellite->par("argPerigee");
  otherParams.trueAnomaly = otherSatellite->par("initialAngle");
  otherParams.eccentricity = otherSatellite->par("eccentricity");

  Position3D otherPos =
      calculateSatellitePositionECEF(otherParams, simTime().dbl());

  return calculateDistance(myPos, otherPos);
}

void Satellite::updateNeighborList() {
  neighbors.clear();

  // Iterate over all outgoing gates "radioOut$o"
  int numGates = gateSize("radioOut$o");

  for (int i = 0; i < numGates; i++) {
    // Get the gate and check where it connects to
    cGate *outGate = gate("radioOut$o", i);

    if (!outGate->isConnected())
      continue;

    // Follow the link to find the destination module
    // Note: Since we have channels (InterSatelliteLink), we need to follow
    // through the channel
    cGate *destGate = outGate->getPathEndGate();
    cModule *destMod = destGate->getOwnerModule();

    // Check if it is a Satellite (ISL - Inter-Satellite Link)
    if (strcmp(destMod->getClassName(), "Satellite") == 0) {
      double distance = calculateDistanceToSatellite(destMod);

      // Update ISL channel delay based on real distance
      cChannel *channel = outGate->getChannel();
      if (channel) {
        // delay = distance / speed_of_light + processing
        double propagationDelay = distance / 299792.458;  // km / (km/s) = seconds
        double processingDelay = 0.001;  // 1ms processing delay
        double totalDelay = propagationDelay + processingDelay;
        channel->par("delay").setDoubleValue(totalDelay);
      }

      // Only add to routing table if within range (simulating link break)
      if (distance <= maxISLRange) {
        NeighborInfo neighbor;
        neighbor.module = destMod;
        neighbor.distance = distance;
        neighbor.gateIndex = i;
        neighbors.push_back(neighbor);
      }
    }
    // Or GroundStation
    else if (strcmp(destMod->getClassName(), "GroundStation") == 0) {
      // Calculate real distance to GS
      GeoCoord gsGeo;
      gsGeo.latitude = destMod->par("latitude").doubleValue();
      gsGeo.longitude = destMod->par("longitude").doubleValue();
      gsGeo.altitude = destMod->par("altitude").doubleValue();
      Position3D gsPos = geoToECEF(gsGeo);
      double distance = calculateDistance(currentPosition, gsPos);

      NeighborInfo neighbor;
      neighbor.module = destMod;
      neighbor.distance = distance;
      neighbor.gateIndex = i;
      neighbors.push_back(neighbor);

      // Add to Routing Table
      int gsAddr = destMod->par("address").intValue();
      RoutingEntry entry;
      entry.destinationId = gsAddr;
      entry.nextHopId = gsAddr;
      entry.cost = distance;
      routingTable.push_back(entry);
    }
  }

  // updateRoutingTable(); // No longer needed to clear/rebuild, we built it
  // incrementally above? Wait, the original code cleared routingTable at
  // start of updateRoutingTable. Let's stick to the original flow:
  // updateNeighborList finds physical neighbors. Then updateRoutingTable()
  // builds the initial table from neighbors.
  updateRoutingTable();
  broadcastRoutingTable();
}

void Satellite::sendToNeighbor(cModule *targetSatellite, cMessage *msg) {

  for (const auto &neighbor : neighbors) {
    if (neighbor.module == targetSatellite) {
      if (gateSize("radioOut$o") > neighbor.gateIndex) {
        sendOrQueue(msg, "radioOut$o", neighbor.gateIndex);
        return;
      }
    }
  }
  EV << "ERROR: Target satellite not in neighbor list! Sender: " << satelliteId
     << " Target Module: " << targetSatellite->getFullName() << endl;
  delete msg;
}

void Satellite::updateRoutingTable() {
  routingTable.clear();

  for (const auto &neighbor : neighbors) {
    RoutingEntry entry;

    if (strcmp(neighbor.module->getClassName(), "Satellite") == 0) {
      entry.destinationId = neighbor.module->par("satelliteId").intValue();
    } else {
      // Must be GroundStation
      entry.destinationId = neighbor.module->par("address").intValue();
    }

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
        int neighborId = -1;
        if (strcmp(neighbor.module->getClassName(), "Satellite") == 0) {
          neighborId = neighbor.module->par("satelliteId").intValue();
        } else {
          neighborId = neighbor.module->par("address").intValue();
        }

        if (neighborId == entry.nextHopId) {
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
      int neighborId = -1;
      if (strcmp(neighbor.module->getClassName(), "Satellite") == 0) {
        neighborId = neighbor.module->par("satelliteId").intValue();
      } else {
        neighborId = neighbor.module->par("address").intValue();
      }

      if (neighborId == msg->sourceId) {
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
