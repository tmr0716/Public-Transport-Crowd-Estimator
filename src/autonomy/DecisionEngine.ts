/**
 * Autonomy Pipeline - Module E: Decision Engine
 * Finite State Machine for Autonomous Collision Avoidance:
 * States: CRUISE | SLOW_DOWN | BRAKE | EMERGENCY_BRAKE | STEER_LEFT | STEER_RIGHT | LANE_CHANGE | FOLLOW | STOP | RESUME
 */

import {
  EgoVehicleState,
  DecisionState,
  PerceptionResult,
  RiskAssessment,
  HazardObject,
  HazardType,
} from '../types/simulation';
import { LaneSafetyStatus } from './LaneChecker';
import { LANE_CENTERS, getLaneCenter } from '../utils/geometry';

export interface DecisionOutput {
  state: DecisionState;
  targetSpeed: number; // m/s
  targetX: number; // Target lateral position (meters)
  targetLane: number;
  indicator: 'none' | 'left' | 'right';
  statusText: string;
  isBraking: boolean;
}

export class DecisionEngine {
  private currentState: DecisionState = 'CRUISE';
  private targetLane: number = 1; // Start in center lane (Lane 1)
  private stateTimer: number = 0;
  private readonly cruiseSpeed = 12.5; // ~45 km/h
  private activeManeuverHazardId: string | null = null;
  private loggedHazards = new Set<string>();

  public onLogEvent?: (
    message: string,
    type: 'detection' | 'planning' | 'action' | 'cleared' | 'system',
    hazardType?: HazardType
  ) => void;

  public reset(): void {
    this.currentState = 'CRUISE';
    this.targetLane = 1;
    this.stateTimer = 0;
    this.activeManeuverHazardId = null;
    this.loggedHazards.clear();
  }

  /**
   * Main decision cycle evaluated every physics step
   */
  public step(
    dt: number,
    ego: EgoVehicleState,
    perceived: PerceptionResult[],
    risk: RiskAssessment,
    laneStatuses: Map<number, LaneSafetyStatus>
  ): DecisionOutput {
    this.stateTimer += dt;
    const primary = risk.primaryHazard;

    // 1. Check for new hazard detection logging
    if (primary && !this.loggedHazards.has(primary.id + '_detected')) {
      this.loggedHazards.add(primary.id + '_detected');
      const dist = Math.round(primary.y - ego.y);
      const capType = primary.type === 'vehicle' ? 'Auto-Rickshaw' : primary.type.charAt(0).toUpperCase() + primary.type.slice(1);
      this.onLogEvent?.(`${capType} detected ${dist}m ahead`, 'detection', primary.type);
    }

    // 2. Decision Logic based on Primary Hazard and Current State
    switch (this.currentState) {
      case 'CRUISE': {
        if (!primary || !risk.isThreat) {
          return {
            state: 'CRUISE',
            targetSpeed: this.cruiseSpeed,
            targetX: getLaneCenter(this.targetLane, ego.y),
            targetLane: this.targetLane,
            indicator: 'none',
            statusText: 'Cruising at normal speed',
            isBraking: false,
          };
        }

        // Dynamic crossing hazard: Animal or Pedestrian
        if (primary.type === 'animal' || primary.type === 'pedestrian') {
          this.transitionTo('BRAKE');
          this.onLogEvent?.('Speed reduced   Controlled braking initiated', 'action', primary.type);
          return {
            state: 'BRAKE',
            targetSpeed: Math.max(0, ego.speed * 0.35),
            targetX: ego.x,
            targetLane: this.targetLane,
            indicator: 'none',
            statusText: `Braking for crossing ${primary.type}`,
            isBraking: true,
          };
        }

        // Static road surface hazard: Pothole in current lane
        if (primary.type === 'pothole') {
          const leftLaneSafe = laneStatuses.get(0)?.isClear ?? false;
          const rightLaneSafe = laneStatuses.get(2)?.isClear ?? false;

          if (leftLaneSafe) {
            this.targetLane = 0;
            this.activeManeuverHazardId = primary.id;
            this.transitionTo('STEER_LEFT');
            this.onLogEvent?.('Safe corridor identified (Lane 1 -> Lane 0)', 'planning', 'pothole');
            this.onLogEvent?.('Steering initiated   Avoiding damaged asphalt', 'action', 'pothole');
            return {
              state: 'STEER_LEFT',
              targetSpeed: 9.0,
              targetX: getLaneCenter(0, ego.y),
              targetLane: 0,
              indicator: 'left',
              statusText: 'Steering around pothole cluster',
              isBraking: false,
            };
          } else if (rightLaneSafe) {
            this.targetLane = 2;
            this.activeManeuverHazardId = primary.id;
            this.transitionTo('STEER_RIGHT');
            return {
              state: 'STEER_RIGHT',
              targetSpeed: 9.0,
              targetX: getLaneCenter(2, ego.y),
              targetLane: 2,
              indicator: 'right',
              statusText: 'Steering around pothole cluster',
              isBraking: false,
            };
          } else {
            this.transitionTo('SLOW_DOWN');
            return {
              state: 'SLOW_DOWN',
              targetSpeed: 3.0,
              targetX: ego.x,
              targetLane: this.targetLane,
              indicator: 'none',
              statusText: 'Slowing down for road hazard',
              isBraking: true,
            };
          }
        }

        // Slower vehicle ahead in ego corridor (e.g. Auto-Rickshaw)
        if (primary.type === 'vehicle') {
          // Check which adjacent lane is clear to execute safe overtake
          let candidateLane = -1;
          if (this.targetLane === 0) {
            candidateLane = (laneStatuses.get(1)?.isClear ?? false) ? 1 : ((laneStatuses.get(2)?.isClear ?? false) ? 2 : -1);
          } else if (this.targetLane === 1) {
            candidateLane = (laneStatuses.get(0)?.isClear ?? false) ? 0 : ((laneStatuses.get(2)?.isClear ?? false) ? 2 : -1);
          } else {
            candidateLane = (laneStatuses.get(1)?.isClear ?? false) ? 1 : ((laneStatuses.get(0)?.isClear ?? false) ? 0 : -1);
          }

          if (candidateLane !== -1) {
            this.targetLane = candidateLane;
            this.activeManeuverHazardId = primary.id;
            this.transitionTo('LANE_CHANGE');
            this.onLogEvent?.(`Safe corridor identified (Lane ${ego.currentLane} -> Lane ${candidateLane})`, 'planning', 'vehicle');
            this.onLogEvent?.('Lane change initiated   Overtaking auto-rickshaw with 2m+ clearance', 'action', 'vehicle');
            return {
              state: 'LANE_CHANGE',
              targetSpeed: Math.min(ego.speed, Math.max(primary.vy, 8.5)),
              targetX: getLaneCenter(this.targetLane, ego.y),
              targetLane: this.targetLane,
              indicator: candidateLane < ego.currentLane ? 'left' : 'right',
              statusText: 'Changing lanes safely to overtake auto-rickshaw',
              isBraking: false,
            };
          } else {
            this.transitionTo('FOLLOW');
            this.onLogEvent?.('Matching speed to follow auto-rickshaw at safe distance', 'action', 'vehicle');
            return {
              state: 'FOLLOW',
              targetSpeed: Math.max(0, primary.vy),
              targetX: getLaneCenter(this.targetLane, ego.y),
              targetLane: this.targetLane,
              indicator: 'none',
              statusText: 'Following auto-rickshaw at safe 20m buffer',
              isBraking: true,
            };
          }
        }
        break;
      }

      case 'STEER_LEFT':
      case 'STEER_RIGHT': {
        const targetX = getLaneCenter(this.targetLane, ego.y);
        const lateralDistToTarget = Math.abs(ego.x - targetX);

        if (lateralDistToTarget < 0.25 && this.stateTimer > 2.0) {
          const potholePassed = !primary || primary.type !== 'pothole' || (primary.y < ego.y - 2.0);
          if (potholePassed) {
            if (!this.loggedHazards.has('pothole_avoided')) {
              this.loggedHazards.add('pothole_avoided');
              this.onLogEvent?.('Pothole avoided   Path clear', 'cleared', 'pothole');
            }
            this.transitionTo('RESUME');
            return {
              state: 'RESUME',
              targetSpeed: this.cruiseSpeed,
              targetX,
              targetLane: this.targetLane,
              indicator: 'none',
              statusText: 'Avoidance complete, returning to cruise',
              isBraking: false,
            };
          }
        }

        return {
          state: this.currentState,
          targetSpeed: 9.0,
          targetX,
          targetLane: this.targetLane,
          indicator: this.currentState === 'STEER_LEFT' ? 'left' : 'right',
          statusText: 'Steering around pothole cluster',
          isBraking: false,
        };
      }

      case 'LANE_CHANGE': {
        const targetX = getLaneCenter(this.targetLane, ego.y);
        const lateralDistToTarget = Math.abs(ego.x - targetX);
        const longDistToLead = primary ? (primary.y - ego.y) : 999;
        const latClearanceToLead = primary ? Math.abs(ego.x - primary.x) : 999;

        // Controlled speed during lane change: NEVER ram lead auto while transitioning lanes
        let safeTargetSpeed = 10.5;
        let requiresBraking = false;

        if (primary && primary.type === 'vehicle' && latClearanceToLead < 2.0) {
          // Still sharing lateral space with auto-rickshaw: cap speed strictly to lead speed + cushion
          const maxAllowedSpeed = Math.max(0, primary.vy + Math.max(-2.0, (longDistToLead - 16.0) * 0.3));
          if (ego.speed > maxAllowedSpeed) {
            safeTargetSpeed = maxAllowedSpeed;
            requiresBraking = true;
          }
        }

        // Once laterally clear and vehicle passed or safely to the side
        if (lateralDistToTarget < 0.25 && this.stateTimer > 1.8 && latClearanceToLead >= 1.8) {
          const vehiclePassed = !primary || primary.type !== 'vehicle' || (primary.y < ego.y - 2.0);
          if (vehiclePassed || (latClearanceToLead >= 2.4 && ego.y > primary.y)) {
            if (!this.loggedHazards.has('vehicle_avoided')) {
              this.loggedHazards.add('vehicle_avoided');
              this.onLogEvent?.('Auto-rickshaw passed safely   Corridor clear', 'cleared', 'vehicle');
            }
            this.transitionTo('RESUME');
            return {
              state: 'RESUME',
              targetSpeed: this.cruiseSpeed,
              targetX,
              targetLane: this.targetLane,
              indicator: 'none',
              statusText: 'Overtake complete, resuming cruise',
              isBraking: false,
            };
          }
        }

        return {
          state: 'LANE_CHANGE',
          targetSpeed: safeTargetSpeed,
          targetX,
          targetLane: this.targetLane,
          indicator: this.targetLane < ego.currentLane ? 'left' : 'right',
          statusText: 'Changing lanes with active safety clearance',
          isBraking: requiresBraking,
        };
      }

      case 'BRAKE':
      case 'SLOW_DOWN': {
        if (!primary) {
          this.transitionTo('RESUME');
          return {
            state: 'RESUME',
            targetSpeed: this.cruiseSpeed,
            targetX: ego.x,
            targetLane: this.targetLane,
            indicator: 'none',
            statusText: 'Roadway clear, resuming cruise',
            isBraking: false,
          };
        }

        const longDist = primary.y - ego.y;
        const requiredBuffer = risk.safeStoppingBuffer;

        if (longDist <= requiredBuffer + 5.0 || ego.speed < 0.8) {
          this.transitionTo('STOP');
          this.onLogEvent?.(`Vehicle stopped safely before ${primary.type}`, 'action', primary.type);
          return {
            state: 'STOP',
            targetSpeed: 0.0,
            targetX: ego.x,
            targetLane: this.targetLane,
            indicator: 'none',
            statusText: `Vehicle stopped safely before ${primary.type}`,
            isBraking: true,
          };
        }

        return {
          state: 'BRAKE',
          targetSpeed: Math.max(0, (longDist - requiredBuffer) * 0.35),
          targetX: ego.x,
          targetLane: this.targetLane,
          indicator: 'none',
          statusText: `Braking safely before ${primary.type}`,
          isBraking: true,
        };
      }

      case 'STOP': {
        if (!primary || primary.state === 'cleared') {
          this.transitionTo('RESUME');
          return {
            state: 'RESUME',
            targetSpeed: this.cruiseSpeed,
            targetX: ego.x,
            targetLane: this.targetLane,
            indicator: 'none',
            statusText: 'Hazard cleared, resuming cruise',
            isBraking: false,
          };
        }

        const lateralClearance = Math.abs(primary.x - ego.x);
        const isDynamic = primary.type === 'animal' || primary.type === 'pedestrian';

        if (isDynamic && lateralClearance > 2.8) {
          const capType = primary.type.charAt(0).toUpperCase() + primary.type.slice(1);
          if (!this.loggedHazards.has(primary.id + '_cleared')) {
            this.loggedHazards.add(primary.id + '_cleared');
            this.onLogEvent?.(`${capType} cleared carriageway   Path clear`, 'cleared', primary.type);
          }
          this.transitionTo('RESUME');
          return {
            state: 'RESUME',
            targetSpeed: this.cruiseSpeed,
            targetX: ego.x,
            targetLane: this.targetLane,
            indicator: 'none',
            statusText: `${capType} cleared road, resuming cruise`,
            isBraking: false,
          };
        }

        return {
          state: 'STOP',
          targetSpeed: 0.0,
          targetX: ego.x,
          targetLane: this.targetLane,
          indicator: 'none',
          statusText: `Vehicle stopped safely, waiting for ${primary.type} to clear`,
          isBraking: true,
        };
      }

      case 'RESUME': {
        if (ego.speed >= this.cruiseSpeed - 0.5) {
          this.transitionTo('CRUISE');
          return {
            state: 'CRUISE',
            targetSpeed: this.cruiseSpeed,
            targetX: getLaneCenter(this.targetLane, ego.y),
            targetLane: this.targetLane,
            indicator: 'none',
            statusText: 'Cruise speed restored',
            isBraking: false,
          };
        }
        return {
          state: 'RESUME',
          targetSpeed: this.cruiseSpeed,
          targetX: getLaneCenter(this.targetLane, ego.y),
          targetLane: this.targetLane,
          indicator: 'none',
          statusText: 'Accelerating smoothly back to cruise',
          isBraking: false,
        };
      }

      case 'FOLLOW': {
        if (!primary || primary.type !== 'vehicle') {
          this.transitionTo('RESUME');
          return {
            state: 'RESUME',
            targetSpeed: this.cruiseSpeed,
            targetX: getLaneCenter(this.targetLane, ego.y),
            targetLane: this.targetLane,
            indicator: 'none',
            statusText: 'Leading vehicle cleared',
            isBraking: false,
          };
        }

        // Periodically check if adjacent lane has opened up
        const candidateLane = this.targetLane === 0 ? 1 : 0;
        const candidateClear = laneStatuses.get(candidateLane)?.isClear ?? false;

        if (candidateClear && this.stateTimer > 1.5) {
          this.targetLane = candidateLane;
          this.transitionTo('LANE_CHANGE');
          this.onLogEvent?.(`Safe corridor opened   Overtaking in Lane ${candidateLane}`, 'planning', 'vehicle');
          return {
            state: 'LANE_CHANGE',
            targetSpeed: 10.0,
            targetX: getLaneCenter(candidateLane, ego.y),
            targetLane: candidateLane,
            indicator: candidateLane < ego.currentLane ? 'left' : 'right',
            statusText: 'Initiating lane change to overtake',
            isBraking: false,
          };
        }

        const longDist = primary.y - ego.y;
        const targetDist = 18.0; // Generous 18m buffer
        const distErr = longDist - targetDist;
        const followSpeed = Math.max(0, primary.vy + distErr * 0.35);

        return {
          state: 'FOLLOW',
          targetSpeed: followSpeed,
          targetX: getLaneCenter(this.targetLane, ego.y),
          targetLane: this.targetLane,
          indicator: 'none',
          statusText: 'Following slower vehicle at safe 18m buffer',
          isBraking: followSpeed < ego.speed,
        };
      }

      default: {
        return {
          state: 'CRUISE',
          targetSpeed: this.cruiseSpeed,
          targetX: getLaneCenter(this.targetLane, ego.y),
          targetLane: this.targetLane,
          indicator: 'none',
          statusText: 'Normal cruising',
          isBraking: false,
        };
      }
    }

    return {
      state: this.currentState,
      targetSpeed: this.cruiseSpeed,
      targetX: getLaneCenter(this.targetLane, ego.y),
      targetLane: this.targetLane,
      indicator: 'none',
      statusText: 'Normal cruising',
      isBraking: false,
    };
  }

  private transitionTo(newState: DecisionState): void {
    if (this.currentState !== newState) {
      this.currentState = newState;
      this.stateTimer = 0;
    }
  }

  public getCurrentState(): DecisionState {
    return this.currentState;
  }
}
