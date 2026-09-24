/**
 * Autonomy Pipeline - Module G: Safety Supervisor & Collision Invariant Enforcer
 */

import { EgoVehicleState, HazardObject } from '../types/simulation';
import { computeOBB, testOBBOverlap, approximateClearance } from '../utils/geometry';

export interface SafetyCheckResult {
  collisionDetected: boolean;
  collisionsCount: number;
  minClearanceOverall: number;
  hazardClearances: Map<string, number>;
  emergencyOverrideTriggered: boolean;
}

export class SafetySupervisor {
  private collisionsCount = 0;
  private minClearanceOverall = Infinity;
  private hazardMinClearances = new Map<string, number>();

  public reset(): void {
    this.collisionsCount = 0;
    this.minClearanceOverall = Infinity;
    this.hazardMinClearances.clear();
  }

  /**
   * Evaluates ego vehicle against all active hazards every physics step
   */
  public evaluate(ego: EgoVehicleState, hazards: HazardObject[]): SafetyCheckResult {
    const egoOBB = computeOBB(ego.x, ego.y, ego.width, ego.length, ego.heading);
    let emergencyOverride = false;

    for (const h of hazards) {
      if (h.state === 'cleared') continue;

      const hazardOBB = computeOBB(h.x, h.y, h.width, h.length, h.heading);
      const isOverlap = testOBBOverlap(egoOBB, hazardOBB);

      if (isOverlap) {
        this.collisionsCount++;
        console.error(
          `[CRITICAL SAFETY BREACH] Collision detected with ${h.type} at world (${ego.x.toFixed(2)}, ${ego.y.toFixed(2)})!`
        );
      }

      const clearance = approximateClearance(egoOBB, hazardOBB);
      if (clearance < this.minClearanceOverall) {
        this.minClearanceOverall = clearance;
      }

      const prevMin = this.hazardMinClearances.get(h.id) ?? Infinity;
      if (clearance < prevMin) {
        this.hazardMinClearances.set(h.id, clearance);
        h.minRecordedDistance = clearance;
      }

      // Hard kinematic safety invariant:
      // If ego is directly in line with hazard and distance is closing within safety buffer, enforce emergency brake
      const longDist = h.y - ego.y;
      const latDist = Math.abs(h.x - ego.x);
      const inDirectLine = latDist < (ego.width * 0.5 + h.width * 0.5 + 0.3);

      if (inDirectLine && longDist > 0 && longDist < 7.5 && ego.speed > Math.max(0, h.vy)) {
        emergencyOverride = true;
        // Hard clamp speed to match or be slower than lead vehicle
        ego.speed = Math.max(0, Math.min(ego.speed - 9.0 * (1 / 60), h.vy * 0.9));
        ego.isBraking = true;
      }
    }

    return {
      collisionDetected: this.collisionsCount > 0,
      collisionsCount: this.collisionsCount,
      minClearanceOverall: this.minClearanceOverall === Infinity ? 0 : this.minClearanceOverall,
      hazardClearances: this.hazardMinClearances,
      emergencyOverrideTriggered: emergencyOverride,
    };
  }

  public getCollisions(): number {
    return this.collisionsCount;
  }

  public getMinClearance(): number {
    return this.minClearanceOverall === Infinity ? 0 : this.minClearanceOverall;
  }
}
