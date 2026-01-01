struct Position3D {
  double x, y, z;
};

struct OrbitParams {
  double altitude;
  double inclination;
  double period;
  double initialAngle;
};

Position3D updateLEOOrbit(const OrbitParams &params, double time);
double calculateDistance(const Position3D &p1, const Position3D &p2);