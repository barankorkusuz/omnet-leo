// --- Channels ---
channel SatelliteToGroundLink extends ned.DatarateChannel {
    parameters:
        double distance @unit("km") = default(500km);
        double baseLatency @unit("ms") = default(2ms);
        delay = (distance / 299792.458km) * 1s + baseLatency;
        datarate = default(10Gbps);
}

channel InterSatelliteLink extends ned.DatarateChannel {
    parameters:
        double distance @unit("km") = default(1000km);
        double baseLatency @unit("ms") = default(5ms);
        delay @mutable = (distance / 299792.458km) * 1s + baseLatency;  // Updated dynamically
        datarate = default(10Gbps);
}

// --- Modules ---
simple Satellite{
    parameters:
        @display("i=device/satellite");
        int satelliteId;
        double altitude @unit("km") = default(500km);
        double inclination @unit("deg") = default(53deg);
        double raan @unit("deg") = default(0deg);
        double argPerigee @unit("deg") = default(0deg);
        double initialAngle @unit("deg") = default(0deg);
        double eccentricity = default(0);
        
        double maxISLRange @unit("km") = default(5000km);
        volatile double sendInterval @unit("s") = default(uniform(1s, 5s));
        int packetSize @unit("B") = default(1024B);

    gates:
        inout radioIn[];
        inout radioOut[];
}

simple GroundStation
{
    parameters:
        @display("i=device/antennatower");
        string locationName = default("Unknown");
        int address = default(0); // Unique Network Address
        double latitude @unit("deg");
        double longitude @unit("deg");
        double altitude @unit("km") = default(0km);
        double maxRange @unit("km") = default(2500km);
        
        // Traffic Generation: How often do we send a message?
        volatile double sendInterval @unit("s") = default(uniform(1s, 10s));
        int packetSize @unit("B") = default(1024B);

    gates:
        inout groundLink[];
}

network LEONetwork {
    parameters:
        @display("bgb=1000,500;bgi=earth,s");
        int numPlanes = default(3);
        int satsPerPlane = default(6);
        // Total Sats = 18 (for Turkey 24/7 coverage)

    submodules:
        // 18 Satellites in 3 orbital planes (Walker 50:18/3/1)
        sat[numPlanes * satsPerPlane]: Satellite {
            parameters:
                satelliteId = index + 1;
                altitude = 550km;
                inclination = 50deg;  // Optimized for Turkey (36-42°N)
                // RAAN: 0°, 60°, 120° for each plane
                raan = (index / 6) * 60deg;
                // Initial angle: 60° spacing within plane + 20° phase offset between planes
                initialAngle = ((index % 6) * 60deg) + ((index / 6) * 20deg);
        }

        // --- Istanbul (Hub) ---
        istanbul: GroundStation {
            locationName = "Istanbul";
            address = 99;
            latitude = 41.0082deg;
            longitude = 28.9784deg;
        }

        // --- Top 10 Hometowns in Istanbul (TUIK 2024) ---
        
        sivas: GroundStation {
            locationName = "Sivas";
            address = 101;
            latitude = 39.7477deg;
            longitude = 37.0179deg;
        }
        kastamonu: GroundStation {
            locationName = "Kastamonu";
            address = 102;
            latitude = 41.3887deg;
            longitude = 33.7827deg;
        }
        ordu: GroundStation {
            locationName = "Ordu";
            address = 103;
            latitude = 40.9839deg;
            longitude = 37.8764deg;
        }
        giresun: GroundStation {
            locationName = "Giresun";
            address = 104;
            latitude = 40.9128deg;
            longitude = 38.3895deg;
        }
        tokat: GroundStation {
            locationName = "Tokat";
            address = 105;
            latitude = 40.3167deg;
            longitude = 36.5500deg;
        }
        erzurum: GroundStation {
            locationName = "Erzurum";
            address = 106;
            latitude = 39.9043deg;
            longitude = 41.2679deg;
        }
        malatya: GroundStation {
            locationName = "Malatya";
            address = 107;
            latitude = 38.3552deg;
            longitude = 38.3095deg;
        }
        samsun: GroundStation {
            locationName = "Samsun";
            address = 108;
            latitude = 41.2867deg;
            longitude = 36.3300deg;
        }
        trabzon: GroundStation {
            locationName = "Trabzon";
            address = 109;
            latitude = 41.0027deg;
            longitude = 39.7168deg;
        }
        sinop: GroundStation {
            locationName = "Sinop";
            address = 110;
            latitude = 42.0231deg;
            longitude = 35.1531deg;
        }

    connections allowunconnected:
        // --- Intra-Plane ISL (Ring within each orbital plane) ---
        // Plane 0: sat[0-5]
        for i=0..5 {
            sat[i].radioOut$o++ --> InterSatelliteLink --> sat[(i+1)%6].radioIn$i++;
            sat[(i+1)%6].radioOut$o++ --> InterSatelliteLink --> sat[i].radioIn$i++;
        }
        // Plane 1: sat[6-11]
        for i=0..5 {
            sat[6+i].radioOut$o++ --> InterSatelliteLink --> sat[6+((i+1)%6)].radioIn$i++;
            sat[6+((i+1)%6)].radioOut$o++ --> InterSatelliteLink --> sat[6+i].radioIn$i++;
        }
        // Plane 2: sat[12-17]
        for i=0..5 {
            sat[12+i].radioOut$o++ --> InterSatelliteLink --> sat[12+((i+1)%6)].radioIn$i++;
            sat[12+((i+1)%6)].radioOut$o++ --> InterSatelliteLink --> sat[12+i].radioIn$i++;
        }

        // --- Inter-Plane ISL (Cross-links between adjacent planes) ---
        for i=0..5 {
            // Plane 0 <-> Plane 1
            sat[i].radioOut$o++ --> InterSatelliteLink --> sat[6+i].radioIn$i++;
            sat[6+i].radioOut$o++ --> InterSatelliteLink --> sat[i].radioIn$i++;
            // Plane 1 <-> Plane 2
            sat[6+i].radioOut$o++ --> InterSatelliteLink --> sat[12+i].radioIn$i++;
            sat[12+i].radioOut$o++ --> InterSatelliteLink --> sat[6+i].radioIn$i++;
            // Plane 2 <-> Plane 0 (closing the mesh)
            sat[12+i].radioOut$o++ --> InterSatelliteLink --> sat[i].radioIn$i++;
            sat[i].radioOut$o++ --> InterSatelliteLink --> sat[12+i].radioIn$i++;
        }

        // Ground Station connections are DYNAMIC
        // Created at runtime by GroundStation::performHandover()
}
