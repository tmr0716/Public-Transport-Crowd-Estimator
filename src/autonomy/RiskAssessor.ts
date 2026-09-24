/**
 * Autonomy Pipeline - Module C: Risk Assessor
 */

import {
  EgoVehicleState,
  PerceptionResult,
  RiskAssessment,
  RiskLevel,
  HazardObject,
} from '../types/simulation';

export class RiskAssessor {
  private readonly reactionTime = 0.35; // seconds
  private readonly normalBraking = 3.5; // m/s^2
  private readonly emergencyBraking = 7.0; // m/s^2

  /**
   * Calculates kinematic stopping distance for current speed
   */
  public computeStoppingDistance(speed: number, emergency: boolean = false): number {
    const a = emergency ? this.emergencyBraking : this.normalBraking;
    return Math.max(0, speed * this.reactionTime + (speed * speed) / (2 * a));
  }

  /**
   * Evaluates perceived obstacles and determines collision risk
   */
  public assess(ego: EgoVehicleState, perceived: PerceptionResult[]): RiskAssessment {
    const stoppingDist = this.computeStoppingDistance(ego.speed);

    if (perceived.length === 0) {
      return {
        level: 'LOW',
        score: 0.05,
        stoppingDistance: stoppingDist,
        safeStoppingBuffer: 4.0,
        isThreat: false,
        primaryHazard: null,
      };
    }

    let highestRiskScore = 0.0;
    let primaryHazard: HazardObject | null = null;
    let primaryBuffer = 4.0;

    for (const item of perceived) {
      const { hazard, longitudinalDist, lateralDist, closingSpeed, ttc } = item;

      let requiredBuffer = 4.0;
      let lateralCorridorWidth = ego.width + 1.2;

      if (hazard.type === 'pedestrian') {
        requiredBuffer = 4.5;
        lateralCorridorWidth = ego.width + 2.4;
      } else if (hazard.type === 'animal') {
        requiredBuffer = 4.0;
        lateralCorridorWidth = ego.width + 2.4;
      } else if (hazard.type === 'pothole') {
        requiredBuffer = 1.2;
        lateralCorridorWidth = ego.width + 0.9;
      } else if (hazard.type === 'vehicle') {
        requiredBuffer = 5.5;
        lateralCorridorWidth = ego.width + 1.8;
      }

      const isInCorridor = Math.abs(lateralDist) < lateralCorridorWidth;

      // 1. Distance Margin Score
      const distMargin = longitudinalDist - (stoppingDist + requiredBuffer);
      let distScore = 0.0;
      if (distMargin <= 0) {
        distScore = 1.0;
      } else if (distMargin < 35.0) {
        distScore = 1.0 - distMargin / 35.0;
      }

      // 2. TTC Score
      let ttcScore = 0.0;
      if (ttc < 1.8) ttcScore = 1.0;
      else if (ttc < 3.5) ttcScore = 0.85;
      else if (ttc < 6.0) ttcScore = 0.55;
      else if (ttc < 9.0) ttcScore = 0.25;

      const lateralFactor = isInCorridor ? 1.0 : Math.max(0.1, 1.0 - Math.abs(lateralDist) / 6.0);
      const combinedScore = (distScore * 0.6 + ttcScore * 0.4) * lateralFactor;

      if (combinedScore > highestRiskScore) {
        highestRiskScore = combinedScore;
        primaryHazard = hazard;
        primaryBuffer = requiredBuffer;
      }
    }

    let level: RiskLevel = 'LOW';
    if (highestRiskScore >= 0.80) level = 'CRITICAL';
    else if (highestRiskScore >= 0.55) level = 'HIGH';
    else if (highestRiskScore >= 0.25) level = 'MEDIUM';

    return {
      level,
      score: Math.min(1.0, highestRiskScore),
      stoppingDistance: stoppingDist,
      safeStoppingBuffer: primaryBuffer,
      isThreat: highestRiskScore >= 0.25,
      primaryHazard,
    };
  }
}
