/**
 * Autonomy Pipeline - Module D: Lane Safety Checker
 */

import { EgoVehicleState, HazardObject } from '../types/simulation';
import { getRoadGeometry, LANE_WIDTH } from '../utils/geometry';

export interface LaneSafetyStatus {
  laneIndex: number;
  laneCenterX: number;
  isAvailable: boolean; // Geometrically on road
  isClear: boolean;     // Obstacle free
  reason?: string;
}

export class LaneSafetyChecker {
  /**
   * Assesses all 3 lanes (0: Left, 1: Center, 2: Right) for safety
   */
  public evaluateLanes(
    ego: EgoVehicleState,
    hazards: HazardObject[],
    stoppingDist: number
  ): Map<number, LaneSafetyStatus> {
    const statusMap = new Map<number, LaneSafetyStatus>();
    const geom = getRoadGeometry(ego.y);
    const laneW = geom.roadWidth / 3;

    for (let laneIdx = 0; laneIdx < geom.laneCenters.length; laneIdx++) {
      const centerX = geom.laneCenters[laneIdx];

      // 1. Boundary check against dynamic road boundaries
      const laneLeft = centerX - laneW * 0.5;
      const laneRight = centerX + laneW * 0.5;

      if (laneLeft < geom.leftEdge - 0.1 || laneRight > geom.rightEdge + 0.1) {
        statusMap.set(laneIdx, {
          laneIndex: laneIdx,
          laneCenterX: centerX,
          isAvailable: false,
          isClear: false,
          reason: 'Beyond legal road boundary or across physical divider',
        });
        continue;
      }

      // 2. Obstacle clearance check
      let isClear = true;
      let conflictReason = '';
      const rearWindow = 12.0;
      const fwdWindow = Math.max(30.0, stoppingDist + 20.0);

      for (const h of hazards) {
        if (h.state === 'cleared') continue;

        const latDist = Math.abs(h.x - centerX);
        const longDist = h.y - ego.y;
        const inLaneZone = latDist < laneW * 0.55 + Math.max(h.width, h.length) * 0.5;

        if (inLaneZone && longDist > -rearWindow && longDist < fwdWindow) {
          isClear = false;
          conflictReason = `Blocked by ${h.type} at ${longDist.toFixed(1)}m`;
          break;
        }

        // Dynamic hazard crossing prediction
        if ((h.type === 'animal' || h.type === 'pedestrian') && h.vx !== 0) {
          const timeToCross = (centerX - h.x) / h.vx;
          if (timeToCross > 0 && timeToCross < 5.0) {
            const predLongDist = (h.y + h.vy * timeToCross) - ego.y;
            if (predLongDist > -rearWindow && predLongDist < fwdWindow) {
              isClear = false;
              conflictReason = `Predicted intersection with crossing ${h.type}`;
              break;
            }
          }
        }
      }

      statusMap.set(laneIdx, {
        laneIndex: laneIdx,
        laneCenterX: centerX,
        isAvailable: true,
        isClear,
        reason: conflictReason || 'Safe corridor clear',
      });
    }

    return statusMap;
  }
}
