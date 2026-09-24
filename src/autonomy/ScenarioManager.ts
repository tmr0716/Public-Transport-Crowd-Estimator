/**
 * Autonomy Pipeline - Module H: Scenario Manager
 * Deterministic scenario coordinator managing the 4 sequential hazards:
 * 1. Pothole cluster (Damaged asphalt at y ~ 88m)
 * 2. Animal (Crossing Cow at y ~ 168m)
 * 3. Pedestrian (Zebra crossing at y ~ 248m)
 * 4. Slower Vehicle Ahead (Auto-Rickshaw at y ~ 325m)
 */

import { HazardObject, EgoVehicleState } from '../types/simulation';
import { getRoadGeometry, getLaneCenter } from '../utils/geometry';

export class ScenarioManager {
  private hazards: HazardObject[] = [];
  private isFinished = false;

  constructor() {
    this.initScenario();
  }

  public reset(): void {
    this.isFinished = false;
    this.initScenario();
  }

  private initScenario(): void {
    // 1. Pothole Cluster (Lane 1 - Center lane with road curvature)
    const potholeY = 88.0;
    const pothole: HazardObject = {
      id: 'hazard_pothole_1',
      type: 'pothole',
      name: 'Asphalt Pothole Cluster',
      x: getLaneCenter(1, potholeY),
      y: potholeY,
      vx: 0,
      vy: 0,
      heading: 0,
      width: 1.8,
      length: 2.2,
      state: 'approaching',
      minRecordedDistance: Infinity,
      behavior: 'static',
    };

    // 2. Animal (Desi Cow crossing from right shoulder towards median)
    const cowY = 168.0;
    const geomCow = getRoadGeometry(cowY);
    const cow: HazardObject = {
      id: 'hazard_cow_1',
      type: 'animal',
      name: 'Crossing Cattle (Cow)',
      x: geomCow.rightEdge + 0.8,
      y: cowY,
      vx: 0,
      vy: 0,
      heading: -Math.PI / 2,
      width: 0.85,
      length: 2.2,
      state: 'approaching',
      minRecordedDistance: Infinity,
      behavior: 'crossing_right_to_left',
      walkPhase: 0,
      initialX: geomCow.rightEdge + 0.8,
      targetX: geomCow.leftEdge - 0.5,
      crossingComplete: false,
    };

    // 3. Pedestrian (Crossing at bottle-neck zebra crossing y = 248m from median to right shoulder)
    const pedY = 248.0;
    const geomPed = getRoadGeometry(pedY);
    const pedestrian: HazardObject = {
      id: 'hazard_pedestrian_1',
      type: 'pedestrian',
      name: 'Pedestrian at Zebra Crossing',
      x: geomPed.leftEdge - 0.4,
      y: pedY,
      vx: 0,
      vy: 0,
      heading: Math.PI / 2,
      width: 0.55,
      length: 0.35,
      state: 'approaching',
      minRecordedDistance: Infinity,
      behavior: 'crossing_left_to_right',
      walkPhase: 0,
      initialX: geomPed.leftEdge - 0.4,
      targetX: geomPed.rightEdge + 0.7,
      crossingComplete: false,
    };

    // 4. Slower Moving Vehicle Ahead (Auto-Rickshaw in Lane 0 at y = 325m)
    const vehicleY = 325.0;
    const slowVehicle: HazardObject = {
      id: 'hazard_vehicle_1',
      type: 'vehicle',
      name: 'Slower Auto-Rickshaw',
      x: getLaneCenter(0, vehicleY),
      y: vehicleY,
      vx: 0,
      vy: 4.5,
      heading: 0,
      width: 1.4,
      length: 2.6,
      state: 'approaching',
      minRecordedDistance: Infinity,
      behavior: 'cruising_slow',
    };

    this.hazards = [pothole, cow, pedestrian, slowVehicle];
  }

  /**
   * Updates hazard positions, crossing behaviors, and walk cycles
   */
  public step(dt: number, ego: EgoVehicleState, simTime: number): void {
    for (const h of this.hazards) {
      if (h.state === 'cleared') continue;

      const longDist = h.y - ego.y;

      // 1. Animal dynamic movement
      if (h.type === 'animal') {
        if (longDist < 52.0 && !h.crossingComplete) {
          h.vx = -0.75;
          h.walkPhase = (h.walkPhase || 0) + dt * 4.5;
          h.x += h.vx * dt;
          if (h.x <= (h.targetX || -3.0)) {
            h.crossingComplete = true;
            h.vx = 0;
            h.state = 'cleared';
          }
        }
      }

      // 2. Pedestrian dynamic movement at zebra crossing
      if (h.type === 'pedestrian') {
        if (longDist < 48.0 && !h.crossingComplete) {
          h.vx = 1.25;
          h.walkPhase = (h.walkPhase || 0) + dt * 6.5;
          h.x += h.vx * dt;
          if (h.x >= (h.targetX || 9.8)) {
            h.crossingComplete = true;
            h.vx = 0;
            h.state = 'cleared';
          }
        }
      }

      // 3. Slower vehicle movement
      if (h.type === 'vehicle') {
        h.y += h.vy * dt;
        h.x = getLaneCenter(0, h.y);
        if (ego.y - h.y > 15.0) {
          h.state = 'cleared';
        }
      }

      // 4. Mark static hazards cleared once ego is past them
      if (h.type === 'pothole' && ego.y - h.y > 10.0) {
        h.state = 'cleared';
      }
    }

    const allCleared = this.hazards.every((h) => h.state === 'cleared');
    if (allCleared && simTime > 28.5) {
      this.isFinished = true;
    }
  }

  public getHazards(): HazardObject[] {
    return this.hazards;
  }

  public isScenarioFinished(): boolean {
    return this.isFinished;
  }
}
