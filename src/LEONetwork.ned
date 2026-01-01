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
        delay = (distance / 299792.458km) * 1s + baseLatency;
        datarate = default(10Gbps);
}

// --- Modules ---
simple Satellite{
    parameters:
        @display("i=device/satellite"); // Satellite icon
        int satelliteId;
        double altitude @unit("km") = default(500km);
        double inclination @unit("deg") = default(53deg); // Starlink-ish
        double raan @unit("deg") = default(0deg);
        double argPerigee @unit("deg") = default(0deg);
        double initialAngle @unit("deg") = default(0deg);
        double eccentricity = default(0);
        
        double maxISLRange @unit("km") = default(5000km);
        volatile double sendInterval @unit("s") = default(uniform(1s, 5s));

    gates:
        inout radioIn[];  // Dynamic gate array
        inout radioOut[]; // Dynamic gate array
}

simple GroundStation
{
    parameters:
        @display("i=device/antennatower"); // More common antenna icon
        string locationName = default("Unknown");
        double latitude @unit("deg");
        double longitude @unit("deg");
        double altitude @unit("km") = default(0km);
        double maxRange @unit("km") = default(2500km);

    gates:
        inout groundLink[];
}

network LEONetwork {
    parameters:
        @display("bgb=1000,500;bgi=earth,s"); // s = stretch to fit
        int numPlanes = default(1);
        int satsPerPlane = default(6);
        // Total Sats = 6

    submodules:
        // 6 Satellites (1 Plane x 6 Sats)
        sat[numPlanes * satsPerPlane]: Satellite {
            parameters:
                satelliteId = index + 1;
                altitude = 550km;
                inclination = 53deg;
                // Single Plane: All RAAN 0
                raan = 0deg;
                // In-Plane Distribution (Anomaly): 0, 60, 120... deg
                initialAngle = (index % 6) * (360deg / 6);
        }

        // Turkey Ground Station Network
        istanbul: GroundStation {
            locationName = "Istanbul";
            latitude = 41.0082deg;
            longitude = 28.9784deg;
        }
        ankara: GroundStation {
            locationName = "Ankara";
            latitude = 39.9334deg;
            longitude = 32.8597deg;
        }
        hakkari: GroundStation {
            locationName = "Hakkari";
            latitude = 37.5744deg;
            longitude = 43.7408deg;
        }

    connections allowunconnected:
        // --- Inter-Satellite Links (ISL) Ring Topology ---
        for i=0..5 {
            // 1. Intra-Plane Link (Connect to Next Sat in Ring)
            sat[i].radioOut$o++ --> InterSatelliteLink --> sat[ int((i+1)%6) ].radioIn$i++;
            sat[ int((i+1)%6) ].radioOut$o++ --> InterSatelliteLink --> sat[i].radioIn$i++;
        }

        // --- Ground Station Uplinks/Downlinks ---
        for i=0..5 {
            // Istanbul Links
            istanbul.groundLink$o++ --> SatelliteToGroundLink --> sat[i].radioIn$i++;
            sat[i].radioOut$o++ --> SatelliteToGroundLink --> istanbul.groundLink$i++;
            
            // Ankara Links
            ankara.groundLink$o++ --> SatelliteToGroundLink --> sat[i].radioIn$i++;
            sat[i].radioOut$o++ --> SatelliteToGroundLink --> ankara.groundLink$i++;

            // Hakkari Links
            hakkari.groundLink$o++ --> SatelliteToGroundLink --> sat[i].radioIn$i++;
            sat[i].radioOut$o++ --> SatelliteToGroundLink --> hakkari.groundLink$i++;
        }
}