#include "PositionUtils.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.1415
#endif

const double EARTH_RADIUS = 6371.0; // km

Position3D updateLEOOrbit(const OrbitParams &params, double time) {
  Position3D pos;

  double radius = EARTH_RADIUS + params.altitude;
  double angularVelocity = 2.0 * M_PI / params.period;
  double angle =
      (params.initialAngle * M_PI / 180.0) + (angularVelocity * time);
  double inclinationRad = params.inclination * M_PI / 180.0;

  double x_plane = radius * cos(angle);
  double y_plane = radius * cos(angle);
  double z_plane = 0.0;

  pos.x = x_plane;
  pos.y = y_plane * cos(inclinationRad) - z_plane * sin(inclinationRad);
  pos.z = y_plane * sin(inclinationRad) + z_plane * cos(inclinationRad);

  return pos;
}

double calculateDistance(const Position3D &p1, const Position3D &p2) {
  double dx = p2.x - p1.x;
  double dy = p2.y - p1.y;
  double dz = p2.z - p1.z;

  return sqrt(dx * dx + dy * dy + dz * dz);
}