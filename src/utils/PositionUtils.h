#ifndef __MY_LEO_POSITIONUTILS_H
#define __MY_LEO_POSITIONUTILS_H

#include <cmath>

// --- Constants ---
const double EARTH_RADIUS = 6371.0;          // km
const double EARTH_GRAVITATIONAL_MU = 398600.4418; // km^3/s^2
const double EARTH_ROTATION_RATE = 7.2921159e-5;   // rad/s (approx)

struct OrbitParams {
  // Keplerian Elements
  double semiMajorAxis;    // a (km) = EarthRadius + Altitude
  double eccentricity;     // e (0 = circular, 0 < e < 1 = elliptical)
  double inclination;      // i (degrees)
  double raan;             // Right Ascension of Ascending Node (degrees)
  double argPerigee;       // Argument of Perigee (degrees)
  double trueAnomaly;      // nu (degrees) - Initial position in orbit
};

struct GeoCoord {
    double latitude;  // degrees
    double longitude; // degrees
    double altitude;  // km
};

struct Position3D {
  double x, y, z;
};

// 1. Convert Lat/Lon (Fixed on Earth) to ECEF (Rotating with Earth) at t=0
Position3D geoToECEF(const GeoCoord &geo);

// 2. Rotate an ECEF position based on Earth's rotation
Position3D rotateWithEarth(const Position3D &initialECEF, double time);

// 3. Propagate Satellite Orbit in ECI and convert to ECEF
Position3D calculateSatellitePositionECEF(const OrbitParams &params, double time);

// --- New Visualization Utils ---
// Convert ECEF back to Lat/Lon (for map display)
GeoCoord ecefToGeo(const Position3D &pos);

// Convert Lat/Lon to Screen X/Y (Mercator Projection)
// mapWidth/Height defines the canvas size (e.g., 1000x500)
Position3D geoToScreen(const GeoCoord &geo, double mapWidth, double mapHeight);

// Helper: Distance calculation
double calculateDistance(const Position3D &p1, const Position3D &p2);

#endif