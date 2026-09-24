/**
 * Autonomy Pipeline - Module A: Perception System
 */

import { EgoVehicleState, HazardObject, PerceptionResult } from '../types/simulation';

export const SENSOR_RANGE_METERS = 75.0;
export const SENSOR_HALF_FOV_RAD = (40 * Math.PI) / 180; // 40 degrees

export class PerceptionSystem {
  /**
   * Scans active hazards and returns perceived objects within sensor envelope
   */
  public process(ego: EgoVehicleState, hazards: HazardObject[]): PerceptionResult[] {
    const results: PerceptionResult[] = [];

    for (const hazard of hazards) {
      if (hazard.state === 'cleared') continue;

      const dx = hazard.x - ego.x; // Lateral offset
      const dy = hazard.y - ego.y; // Longitudinal offset

      // Only perceive objects generally ahead (-6m to +75m)
      if (dy < -6.0 || dy > SENSOR_RANGE_METERS + 5.0) continue;

      const distance = Math.hypot(dx, dy);

      // Check FOV angle relative to ego heading
      const angleToObj = Math.atan2(dx, dy);
      const angleDiff = Math.abs(angleToObj - ego.heading);

      // Within forward FOV or within close adjacent lane bubble (< 20m)
      const inFov = angleDiff <= SENSOR_HALF_FOV_RAD || (distance < 20.0 && Math.abs(dx) < 6.0);

      if (distance <= SENSOR_RANGE_METERS && inFov) {
        const closingSpeed = Math.max(0, ego.speed - hazard.vy);
        const effectiveDist = Math.max(0.1, dy - (ego.length / 2 + hazard.length / 2));
        const ttc = closingSpeed > 0.2 ? effectiveDist / closingSpeed : Infinity;

        results.push({
          hazard,
          distance,
          longitudinalDist: dy,
          lateralDist: dx,
          closingSpeed,
          ttc,
          inFov: true,
        });
      }
    }

    // Sort by longitudinal distance (closest ahead first)
    return results.sort((a, b) => a.longitudinalDist - b.longitudinalDist);
  }
}
