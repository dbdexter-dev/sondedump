#ifndef ecef_h
#define ecef_h

#define WGS84_A 6378137.0
#define WGS84_F (1 / 298.257223563)
#define WGS84_B (WGS84_A * (1 - WGS84_F))
#define WGS84_E_SQR ((WGS84_A*WGS84_A - WGS84_B*WGS84_B)/(WGS84_A*WGS84_A))
#define WGS84_E (sqrtf(WGS84_E_SQR))
#define WGS84_E_PRIME_SQR ((WGS84_A*WGS84_A - WGS84_B*WGS84_B)/(WGS84_B*WGS84_B))
#define WGS84_E_PRIME (sqrtf(WGS84_E_PRIME))

/**
 * Convert ECEF coordinates to lat/lon/alt
 *
 * @param lat pointer latitude, in degrees
 * @param lon pointer to longitude, in degrees
 * @param alt pointer to altitude, in meters
 * @param x ECEF x coordinate
 * @param y ECEF y coordinate
 * @param z ECEF z coordinate
 *
 * @return 0 on success, 1 on invalid xyz coordinates
 */
int ecef_to_lla(float *lat, float *lon, float *alt, float x, float y, float z);

/**
 * Convert ECEF velocity vector to speed/heading/climb rate
 *
 * @param speed pointer to speed, in m/s
 * @param heading pointer to heading, in degrees (0..360)
 * @param v_climb pointer to vertical climb, in m/s
 * @param lat latitude, in degrees
 * @param lon longitude, in degrees
 * @param dx ECEF velocity x component
 * @param dy ECEF velocity y component
 * @param dz ECEF velocity z component
 */
int ecef_to_spd_hdg(float *speed, float *heading, float *v_climb, float lat, float lon, float dx, float dy, float dz);



/**
 * Map lat/lon/alt coordinates to azimuth/elevation/slant range, given the
 * latitude/longitude/altitude of a reference
 *
 * @param az pointer to azimuth, in degrees (0..360)
 * @param el pointer to elevation, in degrees (-90..90)
 * @param slant pointer to slant range (meters)
 * @param lat object latitude (degrees)
 * @param lon object longitude (degrees)
 * @param alt object altitude (meters)
 * @param lat_0 reference latitude (degrees)
 * @param lon_0 reference longitude (degrees)
 * @param alt_0 reference altitude (meters)
 */
int lla_to_aes(float *az, float *el, float *slant, float lat, float lon, float alt, float lat_0, float lon_0, float alt_0);

#endif
