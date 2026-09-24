/**
 * Autonomy Pipeline - Module B: Predictor
 */

import { EgoVehicleState, HazardObject } from '../types/simulation';

export interface TrajectoryPoint {
  t: number; // Time in seconds
  x: number;
  y: number;
}

export interface PredictedHazardPath {
  hazardId: string;
  points: TrajectoryPoint[];
  inflatedRadius: number;
}

export class TrajectoryPredictor {
  private readonly horizonSeconds = 4.0;
  private readonly timeStep = 0.1;

  /**
   * Predicts future positions of a hazard object over the planning horizon
   */
  public predictHazard(hazard: HazardObject): PredictedHazardPath {
    const points: TrajectoryPoint[] = [];
    const steps = Math.floor(this.horizonSeconds / this.timeStep);
    let currX = hazard.x;
    let currY = hazard.y;

    let safetyMargin = 1.0;
    if (hazard.type === 'pedestrian') safetyMargin = 1.8;
    if (hazard.type === 'animal') safetyMargin = 2.0;
    if (hazard.type === 'pothole') safetyMargin = 0.8;
    if (hazard.type === 'vehicle') safetyMargin = 1.5;

    for (let i = 0; i <= steps; i++) {
      const t = i * this.timeStep;
      points.push({
        t,
        x: currX + hazard.vx * t,
        y: currY + hazard.vy * t,
      });
    }

    return {
      hazardId: hazard.id,
      points,
      inflatedRadius: Math.max(hazard.width, hazard.length) * 0.5 + safetyMargin,
    };
  }

  /**
   * Checks if a predicted hazard path will intersect with ego's intended corridor
   */
  public willIntersectCorridor(
    ego: EgoVehicleState,
    predicted: PredictedHazardPath,
    corridorX: number,
    corridorWidth: number
  ): boolean {
    const halfCorridor = corridorWidth * 0.5 + predicted.inflatedRadius;
    for (const pt of predicted.points) {
      const egoPredY = ego.y + ego.speed * pt.t;
      const longGap = Math.abs(pt.y - egoPredY);
      const latGap = Math.abs(pt.x - corridorX);
      if (longGap < ego.length * 1.5 && latGap < halfCorridor) {
        return true;
      }
    }
    return false;
  }
}
