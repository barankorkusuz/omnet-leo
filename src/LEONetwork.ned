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
        int numPlanes = default(1);
        int satsPerPlane = default(6);
        // Total Sats = 6

    submodules:
        // 6 Satellites
        sat[numPlanes * satsPerPlane]: Satellite {
            parameters:
                satelliteId = index + 1;
                altitude = 550km;
                inclination = 53deg;
                raan = 0deg;
                initialAngle = (index % 6) * (360deg / 6);
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
        // --- ISL Ring ---
        for i=0..5 {
            sat[i].radioOut$o++ --> InterSatelliteLink --> sat[ int((i+1)%6) ].radioIn$i++;
            sat[ int((i+1)%6) ].radioOut$o++ --> InterSatelliteLink --> sat[i].radioIn$i++;
        }

        // --- Ground Station Connections ---
        // Connecting all 11 stations (Istanbul + 10 Cities) to all satellites
        for i=0..5 {
            // Istanbul
            istanbul.groundLink$o++ --> SatelliteToGroundLink --> sat[i].radioIn$i++;
            sat[i].radioOut$o++ --> SatelliteToGroundLink --> istanbul.groundLink$i++;

            // 1. Sivas
            sivas.groundLink$o++ --> SatelliteToGroundLink --> sat[i].radioIn$i++;
            sat[i].radioOut$o++ --> SatelliteToGroundLink --> sivas.groundLink$i++;

            // 2. Kastamonu
            kastamonu.groundLink$o++ --> SatelliteToGroundLink --> sat[i].radioIn$i++;
            sat[i].radioOut$o++ --> SatelliteToGroundLink --> kastamonu.groundLink$i++;

            // 3. Ordu
            ordu.groundLink$o++ --> SatelliteToGroundLink --> sat[i].radioIn$i++;
            sat[i].radioOut$o++ --> SatelliteToGroundLink --> ordu.groundLink$i++;
            
            // 4. Giresun
            giresun.groundLink$o++ --> SatelliteToGroundLink --> sat[i].radioIn$i++;
            sat[i].radioOut$o++ --> SatelliteToGroundLink --> giresun.groundLink$i++;

            // 5. Tokat
            tokat.groundLink$o++ --> SatelliteToGroundLink --> sat[i].radioIn$i++;
            sat[i].radioOut$o++ --> SatelliteToGroundLink --> tokat.groundLink$i++;

            // 6. Erzurum
            erzurum.groundLink$o++ --> SatelliteToGroundLink --> sat[i].radioIn$i++;
            sat[i].radioOut$o++ --> SatelliteToGroundLink --> erzurum.groundLink$i++;

            // 7. Malatya
            malatya.groundLink$o++ --> SatelliteToGroundLink --> sat[i].radioIn$i++;
            sat[i].radioOut$o++ --> SatelliteToGroundLink --> malatya.groundLink$i++;

            // 8. Samsun
            samsun.groundLink$o++ --> SatelliteToGroundLink --> sat[i].radioIn$i++;
            sat[i].radioOut$o++ --> SatelliteToGroundLink --> samsun.groundLink$i++;

            // 9. Trabzon
            trabzon.groundLink$o++ --> SatelliteToGroundLink --> sat[i].radioIn$i++;
            sat[i].radioOut$o++ --> SatelliteToGroundLink --> trabzon.groundLink$i++;

            // 10. Sinop
            sinop.groundLink$o++ --> SatelliteToGroundLink --> sat[i].radioIn$i++;
            sat[i].radioOut$o++ --> SatelliteToGroundLink --> sinop.groundLink$i++;
        }
}
