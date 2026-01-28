#include "PositionUtils.h"
#include <iostream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Convert Degrees to Radians
inline double deg2rad(double deg) { return deg * M_PI / 180.0; }

double calculateDistance(const Position3D &p1, const Position3D &p2) {
  double dx = p2.x - p1.x;
  double dy = p2.y - p1.y;
  double dz = p2.z - p1.z;
  return sqrt(dx * dx + dy * dy + dz * dz);
}

// Lat/Lon -> Cartesian (ECEF)
// Bu koordinat "Dünya'ya yapışık"tır. t=0 anındaki konumdur.
Position3D geoToECEF(const GeoCoord &geo) {
    Position3D pos;
    double latRad = deg2rad(geo.latitude);
    double lonRad = deg2rad(geo.longitude);
    double r = EARTH_RADIUS + geo.altitude;

    pos.x = r * cos(latRad) * cos(lonRad);
    pos.y = r * cos(latRad) * sin(lonRad);
    pos.z = r * sin(latRad);
    return pos;
}

// Simülasyon zamanı ilerledikçe Dünya döner.
// Yer istasyonu da döner. Ancak ECEF referans sistemi de Dünya ile döndüğü için
// ECEF koordinatlarında GS SABİT KALIR.
// FAKAT: Uyduları ECI (Inertial) hesaplayıp ECEF'e çevireceğimiz için,
// karşılaştırmayı ECEF (Dünya'ya göre sabit) sistemde yapacağız.
// Yani: GS'nin ECEF koordinatı DEĞİŞMEZ. 
// Uydunun ECI koordinatı ECEF'e çevrilirken "Dünya Dönüş Açısı (GST)" kadar ters döndürülür.
//
// DÜZELTME: Bu fonksiyon "ECI" koordinat sisteminde GS'nin nerede olduğunu bulmak isteseydik gerekirdi.
// Biz tüm hesaplamaları ECEF (Dönen Dünya) sisteminde yapacağız.
// O yüzden bu fonksiyonu "Inertial Viewer" (Sabit Kamera) için kullanabiliriz.
// Ancak mesafe ölçümü için GS sabittir, Uydu "kayar".

Position3D rotateWithEarth(const Position3D &initialECEF, double time) {
    // Bu fonksiyon aslında ECEF -> ECI dönüşümüdür.
    // Şimdilik kullanmayacağız çünkü biz ECEF'te kalacağız.
    return initialECEF; 
}


// Solve Kepler's Equation: M = E - e*sin(E) for E (Eccentric Anomaly)
double solveKepler(double M, double e) {
    double E = M; // Initial guess
    for(int i=0; i<10; i++) { // 10 iterations usually enough
        double f = E - e*sin(E) - M;
        double df = 1 - e*cos(E);
        E = E - f/df;
    }
    return E;
}

Position3D calculateSatellitePositionECEF(const OrbitParams &params, double time) {
    // 1. Mean Motion (n) calculation
    double a = params.semiMajorAxis; // km
    double n = sqrt(EARTH_GRAVITATIONAL_MU / (a * a * a)); // rad/s

    // 2. Mean Anomaly (M) at time t
    // M(t) = M0 + n*t. 
    // We assume initial True Anomaly is given, need to convert to M0 first?
    // For simplicity, let's assume params.trueAnomaly is Mean Anomaly at t=0
    double M_0 = deg2rad(params.trueAnomaly);
    double M = M_0 + n * time;

    // 3. Eccentric Anomaly (E)
    double E = solveKepler(M, params.eccentricity);

    // 4. True Anomaly (nu)
    double sqrt_1_e2 = sqrt(1 - params.eccentricity * params.eccentricity);
    double sin_nu = (sqrt_1_e2 * sin(E)) / (1 - params.eccentricity * cos(E));
    double cos_nu = (cos(E) - params.eccentricity) / (1 - params.eccentricity * cos(E));
    double nu = atan2(sin_nu, cos_nu);

    // 5. Radius (r)
    double r = a * (1 - params.eccentricity * cos(E));

    // 6. Position in Orbital Plane (PQW frame)
    // x' = r * cos(nu)
    // y' = r * sin(nu)
    double u = nu + deg2rad(params.argPerigee); // Argument of Latitude
    double x_orbital = r * cos(u);
    double y_orbital = r * sin(u);

    // 7. Rotate to ECI (Earth-Centered Inertial)
    double raan = deg2rad(params.raan);
    double inc = deg2rad(params.inclination);

    double x_eci = x_orbital * cos(raan) - y_orbital * cos(inc) * sin(raan);
    double y_eci = x_orbital * sin(raan) + y_orbital * cos(inc) * cos(raan);
    double z_eci = y_orbital * sin(inc);

    // 8. ECI -> ECEF Transformation (Accounting for Earth Rotation)
    // Greenwich Sidereal Time (GST) angle
    double theta_gst = EARTH_ROTATION_RATE * time; // Radyan

    double x_ecef = x_eci * cos(theta_gst) + y_eci * sin(theta_gst);
    double y_ecef = -x_eci * sin(theta_gst) + y_eci * cos(theta_gst);
    double z_ecef = z_eci;

    Position3D pos;
    pos.x = x_ecef;
    pos.y = y_ecef;
    pos.z = z_ecef;
    return pos;
}

GeoCoord ecefToGeo(const Position3D &pos) {
    GeoCoord geo;
    double r = sqrt(pos.x*pos.x + pos.y*pos.y + pos.z*pos.z);
    
    // Simple Spherical Approximation (sufficient for visualization)
    geo.latitude = asin(pos.z / r) * 180.0 / M_PI;
    geo.longitude = atan2(pos.y, pos.x) * 180.0 / M_PI;
    geo.altitude = r - EARTH_RADIUS;
    
    return geo;
}

Position3D geoToScreen(const GeoCoord &geo, double mapWidth, double mapHeight) {
    Position3D screen;
    
    // Map Longitude (-180..180) to X (0..width)
    // +180 to shift range to 0..360
    screen.x = (geo.longitude + 180.0) * (mapWidth / 360.0);
    
    // Map Latitude (-90..90) to Y (height..0) -> Note: Screen Y is inverted (0 is top)
    // -90 is bottom (height), 90 is top (0)
    screen.y = (90.0 - geo.latitude) * (mapHeight / 180.0);
    
    screen.z = 0;
    return screen;
}
