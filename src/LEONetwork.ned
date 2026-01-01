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
    connections allowunconnected:

    // sat1 <-> sat2 (çift yönlü)
    sat1.radioOut$o[0] --> sat2.radioIn$i[0];
    sat2.radioOut$o[0] --> sat1.radioIn$i[0];
    
    // sat1 <-> sat3 (çift yönlü)
    sat1.radioOut$o[1] --> sat3.radioIn$i[0];
    sat3.radioOut$o[0] --> sat1.radioIn$i[1];
    
    // sat2 <-> sat3 (çift yönlü)
    sat2.radioOut$o[1] --> sat3.radioIn$i[1];
    sat3.radioOut$o[1] --> sat2.radioIn$i[1];

}