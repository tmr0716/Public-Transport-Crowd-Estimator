/**
 * Autonomy Pipeline - Module F: Vehicle Controller & Kinematic Bicycle Physics
 */

import { EgoVehicleState } from '../types/simulation';
import { clamp, lerp, ROAD_LEFT_EDGE, ROAD_RIGHT_EDGE } from '../utils/geometry';

export class VehicleController {
  private readonly maxAccel = 2.2; // m/s^2
  private readonly comfortDecel = 3.5; // m/s^2
  private readonly maxBraking = 8.5; // m/s^2
  private readonly maxSteerRad = (32 * Math.PI) / 180; // ~32 deg in radians (~0.558 rad)
  private readonly maxSteerRate = (60 * Math.PI) / 180; // rad/s

  /**
   * Physics step integrating the vehicle kinematics forward by dt
   */
  public update(
    ego: EgoVehicleState,
    targetSpeed: number,
    targetX: number,
    isBraking: boolean,
    dt: number
  ): void {
    // 1. Longitudinal Control
    const speedError = targetSpeed - ego.speed;
    let desiredAccel = 0;

    if (speedError > 0 && !isBraking) {
      desiredAccel = Math.min(this.maxAccel, speedError * 1.6);
    } else {
      const brakeLimit = isBraking ? this.maxBraking : this.comfortDecel;
      desiredAccel = Math.max(-brakeLimit, speedError * 2.5);
    }

    ego.acceleration = lerp(ego.acceleration, desiredAccel, clamp(dt * 9.0, 0, 1));
    ego.speed = Math.max(0, ego.speed + ego.acceleration * dt);

    // 2. Physical Divider Safety Boundary & Steering Correction
    const minSafeX = ROAD_LEFT_EDGE + ego.width * 0.5 + 0.45;
    const maxSafeX = ROAD_RIGHT_EDGE - ego.width * 0.5 - 0.45;
    const safeTargetX = clamp(targetX, minSafeX, maxSafeX);

    // 3. Lateral Control (Stanley Controller)
    const crossTrackError = safeTargetX - ego.x;
    const headingError = -ego.heading;
    const kStanley = 0.75;
    const vSoft = 1.0;

    let stanleyAngle =
      headingError * 0.9 +
      Math.atan2(kStanley * crossTrackError, Math.max(1.0, ego.speed) + vSoft);

    if (ego.x < minSafeX + 0.25) {
      stanleyAngle = Math.max(stanleyAngle, 0.15);
    } else if (ego.x > maxSafeX - 0.25) {
      stanleyAngle = Math.min(stanleyAngle, -0.15);
    }

    const clampedTargetSteer = clamp(stanleyAngle, -this.maxSteerRad, this.maxSteerRad);
    const steerDiff = clampedTargetSteer - ego.steeringAngle;
    const maxStep = this.maxSteerRate * dt;
    const actualSteerChange = clamp(steerDiff, -maxStep, maxStep);

    ego.steeringAngle += actualSteerChange;

    // 4. Kinematic Bicycle Model Motion Integration
    if (ego.speed > 0.05) {
      const angularVel = (ego.speed / ego.wheelbase) * Math.tan(ego.steeringAngle);
      ego.heading += angularVel * dt;
      ego.x += ego.speed * Math.sin(ego.heading) * dt;
      ego.y += ego.speed * Math.cos(ego.heading) * dt;
    }

    if (ego.x < minSafeX) {
      ego.x = minSafeX;
      if (ego.heading < 0) ego.heading = 0;
    } else if (ego.x > maxSafeX) {
      ego.x = maxSafeX;
      if (ego.heading > 0) ego.heading = 0;
    }

    ego.isBraking = isBraking || desiredAccel < -1.5;
    const targetPitch = ego.isBraking ? clamp(Math.abs(ego.acceleration) * 0.025, 0, 0.08) : 0;
    ego.pitch = lerp(ego.pitch, targetPitch, clamp(dt * 10, 0, 1));
  }
}
