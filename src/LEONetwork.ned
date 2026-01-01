simple Satellite{
    parameters:
        int satelliteId;
        double altitude @unit("km") = default(500km);
        double inclination @unit("deg") = default(53deg);
        double period @unit("s") = default(5676s);
        double initialAngle @unit("deg") = default(0deg);
        double maxISLRange @unit("km") = default(5000km);

    gates:
    inout radioIn[10];
    inout radioOut[10];
}

simple InterSatelliteLink
{
    parameters:
        double distance @unit("km") = default(1000km);
        double dataRate @unit("Gbps") = default(10Gbps);
        double baseLatency @unit("ms") = default(5ms);

    gates:
        input linkIn;
        output linkOut;   
}

simple GroundStation
{
    parameters:
        double x @unit("km") = default(0km);
        double y @unit("km") = default(0km);
        double z @unit("km") = default(0km);

        double maxRange @unit("km") = default(2000km);

    gates:
        inout groundLink[3];
}

simple SatelliteToGroundLink{
    parameters:
        double distance @unit("km") = default(500km);
        double dataRate @unit("Gbps") = default(10Gbps);
        double baseLatency @unit("ms") = default(2ms);
    gates:
        input linkIn;
        output linkOut;
}

network LEONetwork {
    submodules:
        sat1: Satellite{
            satelliteId = 1;
            altitude = 500km;
            inclination = 53deg;
            period = 5676s;
            initialAngle = 0deg;
        }
        sat2: Satellite{
            satelliteId = 2;
            altitude = 500km;
            inclination = 53deg;
            period = 5676s;
            initialAngle = 120deg;
        }
        sat3: Satellite{
            satelliteId = 3;
            altitude = 500km;
            inclination = 53deg;
            period = 5676s;
            initialAngle = 240deg;
        }
        gs1: GroundStation{
            x = 0km;
            y = 0km;
            z = 6371km;
            maxRange = 2000km;
        }
        
        sgl_gs_to_sat1: SatelliteToGroundLink {
            distance = 500km;
            dataRate = 10Gbps;
            baseLatency = 2ms;
        }
        sgl_gs_to_sat2: SatelliteToGroundLink {
            distance = 500km;
            dataRate = 10Gbps;
            baseLatency = 2ms;
        }
        sgl_gs_to_sat3: SatelliteToGroundLink {
            distance = 500km;
            dataRate = 10Gbps;
            baseLatency = 2ms;
        }
        
        // Satellite -> Ground Station links
        sgl_sat1_to_gs: SatelliteToGroundLink {
            distance = 500km;
            dataRate = 10Gbps;
            baseLatency = 2ms;
        }
        sgl_sat2_to_gs: SatelliteToGroundLink {
            distance = 500km;
            dataRate = 10Gbps;
            baseLatency = 2ms;
        }
        sgl_sat3_to_gs: SatelliteToGroundLink {
            distance = 500km;
            dataRate = 10Gbps;
            baseLatency = 2ms;
        }
    connections allowunconnected:
        // sat1 <-> sat2
        sat1.radioOut$o[0] --> sat2.radioIn$i[0];
        sat2.radioOut$o[0] --> sat1.radioIn$i[0];
        // sat1 <-> sat3
        sat1.radioOut$o[1] --> sat3.radioIn$i[0];
        sat3.radioOut$o[0] --> sat1.radioIn$i[1];
        // sat2 <-> sat3
        sat2.radioOut$o[1] --> sat3.radioIn$i[1];
        sat3.radioOut$o[1] --> sat2.radioIn$i[1];


        gs1.groundLink$o[0] --> sgl_gs_to_sat1.linkIn;
        sgl_gs_to_sat1.linkOut --> sat1.radioIn$i[2];
        
        gs1.groundLink$o[1] --> sgl_gs_to_sat2.linkIn;
        sgl_gs_to_sat2.linkOut --> sat2.radioIn$i[2];
        
        gs1.groundLink$o[2] --> sgl_gs_to_sat3.linkIn;
        sgl_gs_to_sat3.linkOut --> sat3.radioIn$i[2];
        
        // Satellite -> Ground Station bağlantıları
        sat1.radioOut$o[2] --> sgl_sat1_to_gs.linkIn;
        sgl_sat1_to_gs.linkOut --> gs1.groundLink$i[0];
        
        sat2.radioOut$o[2] --> sgl_sat2_to_gs.linkIn;
        sgl_sat2_to_gs.linkOut --> gs1.groundLink$i[1];
        
        sat3.radioOut$o[2] --> sgl_sat3_to_gs.linkIn;
        sgl_sat3_to_gs.linkOut --> gs1.groundLink$i[2];

}