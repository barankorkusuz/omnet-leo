#include "GroundStation.h"
#include "DataPacket.h"
#include "omnetpp/checkandcast.h"
#include "omnetpp/cdataratechannel.h"
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

  myAddress = par("address");

  position = geoToECEF(geo);

  // Set initial 2D Map position
  Position3D screenPos = geoToScreen(geo, 1000.0, 500.0);
  getDisplayString().setTagArg("p", 0, (long)screenPos.x);
  getDisplayString().setTagArg("p", 1, (long)screenPos.y);

  maxRange = par("maxRange");
  currentSatellite = nullptr;
  currentSatGateIndex = -1;

  endToEndDelay = new cOutVector("endToEndDelay");
  packetsSent = 0;
  packetsReceived = 0;
  packetsDropped = 0;
  totalBitsReceived = 0;
  firstPacketTime = 0;
  lastPacketTime = 0;

  // Queue Init - Larger Buffer for GS too
  txQueue = new cQueue("txQueue");
  txFinishTimer = new cMessage("txFinishTimer");
  maxQueueSize = 1000; 
  
  // DEBUG: Check actual Packet Size
  int pSize = par("packetSize").intValue();
  EV << "WARNING: GroundStation " << myAddress << " Packet Size is: " << pSize << " Bytes (" << (pSize*8.0)/1000000.0 << " Mb)" << endl;

  EV << "GroundStation " << myAddress << " initialized at position: (" << position.x << ", "
     << position.y << ", " << position.z << ") km" << endl;

  handoverTimer = new cMessage("handoverTimer");
  scheduleAt(simTime() + 1.0, handoverTimer);

  trafficTimer = new cMessage("trafficTimer");
  scheduleAt(simTime() + par("sendInterval"), trafficTimer);

  // perform to find first satellite to connect
  performHandover();
}

void GroundStation::handleMessage(cMessage *msg) {
  if (msg == txFinishTimer) {
      processTxQueue();
  } else if (msg == handoverTimer) { // handover timer
    performHandover();
    scheduleAt(simTime() + 1.0, handoverTimer); // Check handover every 1s
  } else if (msg == trafficTimer) {
    // Generate Packet
    char pktName[32];
    snprintf(pktName, sizeof(pktName), "GS-%d-%ld", myAddress, packetsSent);
    DataPacket *packet = new DataPacket(pktName);
    packet->setBitLength(par("packetSize").intValue() * 8); // Bytes to Bits
    packet->sourceId = myAddress;
    
    // Target Logic
    if (myAddress == 99) {
        // I am Istanbul -> Send to random hometown (101-110)
        packet->destinationId = 101 + intuniform(0, 9);
    } else {
        // I am a Hometown -> Send to Istanbul (99)
        packet->destinationId = 99;
    }
    
    packet->packetId = packetsSent;
    sendToCurrentSatellite(packet);
    
    // Reschedule
    scheduleAt(simTime() + par("sendInterval"), trafficTimer);

  } else if (dynamic_cast<DataPacket *>(msg) != nullptr) {
    // DataPacket received
    DataPacket *packet = check_and_cast<DataPacket *>(msg);
    packetsReceived++;
    totalBitsReceived += packet->getBitLength();
    
    // Time tracking for Throughput
    if (packetsReceived == 1) {
        firstPacketTime = simTime();
    }
    lastPacketTime = simTime();

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

// --- Queue Logic ---
void GroundStation::sendOrQueue(cMessage *msg, const char *gateName, int gateIndex) {
    if (txQueue->getLength() >= maxQueueSize) {
        EV << "GS Tx Queue Full! Dropping packet " << msg->getName() << endl;
        packetsDropped++;
        delete msg;
        return;
    }
    msg->setContextPointer((void*)(intptr_t)gateIndex);
    txQueue->insert(msg);
    processTxQueue();
}

void GroundStation::processTxQueue() {
    if (txQueue->isEmpty()) return;
    if (gateSize("groundLink") == 0) return;

    cGate *outGate = gate("groundLink$o", 0);
    if (!outGate->isConnected()) {
        // No connection, wait for handover
        return;
    }

    cMessage *msg = (cMessage*)txQueue->front();
    cChannel *chan = outGate->getTransmissionChannel();

    if (chan && chan->isBusy()) {
        simtime_t finishTime = chan->getTransmissionFinishTime();
        if (!txFinishTimer->isScheduled()) {
            scheduleAt(finishTime, txFinishTimer);
        }
    } else {
        txQueue->pop();
        send(msg, "groundLink$o", 0);
    }
}

void GroundStation::finish() {
  if (handoverTimer) {
    cancelAndDelete(handoverTimer);
  }
  if (trafficTimer) {
    cancelAndDelete(trafficTimer);
  }
  if (txFinishTimer) {
    cancelAndDelete(txFinishTimer);
  }
  delete txQueue;

  // statistics
  EV << "=== GroundStation " << myAddress << " Statistics ===" << endl;
  EV << "Packets Sent: " << packetsSent << endl;
  EV << "Packets Received: " << packetsReceived << endl;
  EV << "Packets Dropped: " << packetsDropped << endl;

  // --- End-to-End Metrics ---
  double simDuration = simTime().dbl();
  double activeDuration = (lastPacketTime - firstPacketTime).dbl();
  if (activeDuration <= 0.001) activeDuration = simDuration;

  double throughputBps = (activeDuration > 0) ? (double)totalBitsReceived / activeDuration : 0.0;
  
  // PDR for this Node (Rx Success) -> Not fully accurate for global PDR, but good for local
  // For global view, better use Drops.
  
  recordScalar("Throughput_bps", throughputBps);
  recordScalar("PacketsReceived", packetsReceived);
  recordScalar("PacketsSent", packetsSent);
  recordScalar("PacketsDropped", packetsDropped);

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
    // Disconnect from old satellite
    if (currentSatellite) {
      EV << "GroundStation " << myAddress << " handover FROM Satellite "
         << currentSatellite->par("satelliteId").intValue() << endl;
      disconnectFromSatellite();
    }

    currentSatellite = nearestSat;

    // Connect to new satellite
    if (currentSatellite) {
      connectToSatellite(currentSatellite);
      EV << "GroundStation " << myAddress << " handover TO Satellite "
         << currentSatellite->par("satelliteId").intValue() << endl;
    } else {
      EV << "GroundStation " << myAddress << " has NO satellite in range!" << endl;
    }
  }
}

void GroundStation::connectToSatellite(cModule *satellite) {
  // Ensure we have at least 1 gate
  if (gateSize("groundLink") == 0) {
    setGateSize("groundLink", 1);
  }

  // Get our gates
  cGate *gsOutGate = gate("groundLink$o", 0);
  cGate *gsInGate = gate("groundLink$i", 0);

  // Expand satellite gate arrays
  int satGateSize = satellite->gateSize("radioIn");
  satellite->setGateSize("radioIn", satGateSize + 1);
  satellite->setGateSize("radioOut", satGateSize + 1);
  currentSatGateIndex = satGateSize;

  cGate *satInGate = satellite->gate("radioIn$i", currentSatGateIndex);
  cGate *satOutGate = satellite->gate("radioOut$o", currentSatGateIndex);

  // Calculate real distance to satellite for accurate delay
  OrbitParams satOrbit;
  satOrbit.semiMajorAxis = EARTH_RADIUS + satellite->par("altitude").doubleValue();
  satOrbit.inclination = satellite->par("inclination");
  satOrbit.raan = satellite->par("raan");
  satOrbit.argPerigee = satellite->par("argPerigee");
  satOrbit.trueAnomaly = satellite->par("initialAngle");
  satOrbit.eccentricity = satellite->par("eccentricity");

  Position3D satPos = calculateSatellitePositionECEF(satOrbit, simTime().dbl());
  double distance = calculateDistance(position, satPos);  // km

  // Propagation delay = distance / speed_of_light + processing delay
  // Speed of light = 299792.458 km/s
  double propagationDelay = distance / 299792.458;  // seconds
  double processingDelay = 0.001;  // 1ms processing
  double totalDelay = propagationDelay + processingDelay;

  EV << "GS " << myAddress << " -> Sat " << satellite->par("satelliteId").intValue()
     << " distance: " << distance << " km, delay: " << (totalDelay * 1000) << " ms" << endl;

  // Create channels (GS -> Satellite)
  cDatarateChannel *channelToSat = cDatarateChannel::create("gsToSat");
  channelToSat->setDatarate(4e9);  // 4 Gbps
  channelToSat->setDelay(totalDelay);

  // Create channels (Satellite -> GS)
  cDatarateChannel *channelFromSat = cDatarateChannel::create("satToGs");
  channelFromSat->setDatarate(4e9);
  channelFromSat->setDelay(totalDelay);

  // Connect: GS -> Satellite
  gsOutGate->connectTo(satInGate, channelToSat);
  channelToSat->callInitialize();

  // Connect: Satellite -> GS
  satOutGate->connectTo(gsInGate, channelFromSat);
  channelFromSat->callInitialize();

  EV << "Dynamic link created: GS " << myAddress << " <-> Satellite "
     << satellite->par("satelliteId").intValue()
     << " (gate index: " << currentSatGateIndex << ")" << endl;
}

void GroundStation::disconnectFromSatellite() {
  if (gateSize("groundLink") == 0) return;

  cGate *gsOutGate = gate("groundLink$o", 0);
  cGate *gsInGate = gate("groundLink$i", 0);

  // Disconnect outgoing
  if (gsOutGate->isConnected()) {
    gsOutGate->disconnect();
  }

  // Disconnect incoming (from satellite side)
  if (gsInGate->isConnected()) {
    cGate *remoteSrcGate = gsInGate->getPreviousGate();
    if (remoteSrcGate) {
      remoteSrcGate->disconnect();
    }
  }

  currentSatGateIndex = -1;
}
void GroundStation::sendToCurrentSatellite(cMessage *msg) {
  if (!currentSatellite || gateSize("groundLink") == 0) {
    EV << "WARN: No satellite connected to GS " << myAddress << "! Packet dropped." << endl;
    packetsDropped++;
    delete msg;
    return;
  }

  cGate *outGate = gate("groundLink$o", 0);
  if (!outGate->isConnected()) {
    EV << "WARN: GS " << myAddress << " gate not connected! Packet dropped." << endl;
    packetsDropped++;
    delete msg;
    return;
  }

  if (dynamic_cast<DataPacket *>(msg) != nullptr) {
    packetsSent++;
  }

  // Always use gate 0 (single dynamic connection)
  sendOrQueue(msg, "groundLink$o", 0);
}