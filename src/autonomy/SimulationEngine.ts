/**
 * Autonomy Pipeline - Core Simulation Engine
 */

import {
  EgoVehicleState,
  TelemetryData,
  LogEntry,
  SimSettings,
  HazardType,
  HazardObject,
} from '../types/simulation';
import { LANE_CENTERS, getRoadGeometry } from '../utils/geometry';
import { PerceptionSystem } from './Perception';
import { TrajectoryPredictor } from './Predictor';
import { RiskAssessor } from './RiskAssessor';
import { LaneSafetyChecker } from './LaneChecker';
import { DecisionEngine } from './DecisionEngine';
import { VehicleController } from './Controller';
import { SafetySupervisor } from './SafetySupervisor';
import { ScenarioManager } from './ScenarioManager';
import { RoadRenderer } from '../rendering/RoadRenderer';
import { VehicleSpriteRenderer } from '../rendering/VehicleSprite';
import { PotholeRenderer } from '../rendering/PotholeGenerator';
import { HazardSpriteRenderer } from '../rendering/HazardSprites';
import { Realistic3DEnvironment } from '../rendering/Realistic3DEnvironment';

export class SimulationEngine {
  private ego: EgoVehicleState;
  private simTime = 0; // seconds
  private isRunning = false;
  private isPaused = false;
  private isCompleted = false;

  // Autonomy Pipeline Modules
  private perception = new PerceptionSystem();
  private predictor = new TrajectoryPredictor();
  private riskAssessor = new RiskAssessor();
  private laneChecker = new LaneSafetyChecker();
  private decisionEngine = new DecisionEngine();
  private controller = new VehicleController();
  private safetySupervisor = new SafetySupervisor();
  private scenario = new ScenarioManager();

  // Procedural Graphics Renderers
  private environment3D = new Realistic3DEnvironment();
  private roadRenderer = new RoadRenderer();
  private vehicleSprite = new VehicleSpriteRenderer();
  private potholeRenderer = new PotholeRenderer();
  private hazardSprites = new HazardSpriteRenderer();

  // Camera tracking
  private camX = LANE_CENTERS[1];
  private camY = 0;

  // Settings
  public settings: SimSettings = {
    showSensorCone: true,
    showPlannedPath: true,
    showBoundingBoxes: false,
    showTelemetryOverlay: true,
    simSpeedMultiplier: 1.0,
    viewMode: 'cockpit_3d',
  };

  // Event Log
  private logs: LogEntry[] = [];
  private activeBanner: TelemetryData['activeBanner'] = null;

  // Callbacks
  public onTelemetryUpdate?: (telemetry: TelemetryData) => void;
  public onLogsUpdate?: (logs: LogEntry[]) => void;
  public onCompletion?: (telemetry: TelemetryData) => void;

  constructor() {
    this.ego = this.createInitialEgo();
    this.decisionEngine.onLogEvent = (msg, type, hazardType) => {
      this.addLog(msg, type, hazardType);
    };
  }

  private createInitialEgo(): EgoVehicleState {
    return {
      x: LANE_CENTERS[1],
      y: 0.0,
      heading: 0.0,
      speed: 0.0,
      targetSpeed: 12.5, // Cruise ~45 km/h
      acceleration: 0.0,
      steeringAngle: 0.0,
      targetSteeringAngle: 0.0,
      currentLane: 1,
      targetLane: 1,
      width: 1.8,
      length: 4.2,
      wheelbase: 2.6,
      isBraking: false,
      indicator: 'none',
      indicatorTimer: 0,
      pitch: 0,
    };
  }

  public start(): void {
    if (this.isCompleted) {
      this.reset();
    }
    this.isRunning = true;
    this.isPaused = false;
    this.addLog('Autonomous collision avoidance system engaged', 'system');
  }

  public pause(): void {
    if (!this.isRunning) return;
    this.isPaused = !this.isPaused;
  }

  public reset(): void {
    this.isRunning = false;
    this.isPaused = false;
    this.isCompleted = false;
    this.simTime = 0;
    this.ego = this.createInitialEgo();
    this.camX = LANE_CENTERS[1];
    this.camY = 0;
    this.logs = [];
    this.activeBanner = null;
    this.scenario.reset();
    this.decisionEngine.reset();
    this.safetySupervisor.reset();
    this.onLogsUpdate?.([]);
    this.addLog('Simulation reset   System armed and scanning', 'system');
    this.broadcastTelemetry();
  }

  public setSpeedMultiplier(mult: number): void {
    this.settings.simSpeedMultiplier = mult;
  }

  private addLog(
    message: string,
    type: LogEntry['type'],
    hazardType?: HazardType
  ): void {
    const formatted = `[${this.simTime.toFixed(1).padStart(4, '0')}s]`;
    const entry: LogEntry = {
      id: `${this.simTime}_${Math.random().toString(36).substr(2, 6)}`,
      timestamp: this.simTime,
      timeFormatted: formatted,
      message,
      type,
      hazardType,
    };
    this.logs.push(entry);
    this.onLogsUpdate?.([...this.logs]);

    if (type === 'detection' && hazardType) {
      const bannerText = hazardType === 'vehicle' ? 'AUTO-RICKSHAW DETECTED' : `${hazardType.toUpperCase()} DETECTED`;
      this.activeBanner = {
        text: bannerText,
        level: 'HIGH',
        expireAt: this.simTime + 2.4,
      };
    } else if (type === 'cleared' && hazardType) {
      const bannerText = hazardType === 'vehicle' ? 'AUTO-RICKSHAW OVERTAKEN   PATH CLEAR' : `${hazardType.toUpperCase()} AVOIDED   PATH CLEAR`;
      this.activeBanner = {
        text: bannerText,
        level: 'LOW',
        expireAt: this.simTime + 2.0,
      };
    }
  }

  /**
   * Fixed 60Hz Physics step called by the animation loop
   */
  public stepPhysics(dt: number): void {
    if (!this.isRunning || this.isPaused || this.isCompleted) return;

    const effectiveDt = dt * this.settings.simSpeedMultiplier;
    this.simTime += effectiveDt;

    const hazards = this.scenario.getHazards();

    // 1. Perception
    const perceived = this.perception.process(this.ego, hazards);

    // 2. Risk Assessment
    const risk = this.riskAssessor.assess(this.ego, perceived);

    // 3. Lane Safety Evaluation
    const laneStatuses = this.laneChecker.evaluateLanes(this.ego, hazards, risk.stoppingDistance);

    // 4. Decision Engine
    const decision = this.decisionEngine.step(effectiveDt, this.ego, perceived, risk, laneStatuses);
    this.ego.indicator = decision.indicator;
    this.ego.targetLane = decision.targetLane;

    const currentGeom = getRoadGeometry(this.ego.y);
    const div01 = (currentGeom.laneCenters[0] + currentGeom.laneCenters[1]) * 0.5;
    const div12 = (currentGeom.laneCenters[1] + currentGeom.laneCenters[2]) * 0.5;
    if (this.ego.x < div01) this.ego.currentLane = 0;
    else if (this.ego.x > div12) this.ego.currentLane = 2;
    else this.ego.currentLane = 1;

    // 5. Kinematic Bicycle Controller
    this.controller.update(
      this.ego,
      decision.targetSpeed,
      decision.targetX,
      decision.isBraking,
      effectiveDt
    );

    // 6. Safety Supervisor (Enforces zero-collision invariant)
    this.safetySupervisor.evaluate(this.ego, hazards);

    // 7. Update Scenario & Dynamic Hazards
    this.scenario.step(effectiveDt, this.ego, this.simTime);
    this.roadRenderer.updateAmbientTraffic(effectiveDt, this.ego.y);
    this.environment3D.update(effectiveDt, this.ego.y);

    // Smooth Camera Follow
    this.camX += (this.ego.x - this.camX) * 0.08;
    this.camY += (this.ego.y - this.camY) * 0.08;

    // Check completion condition
    if (this.scenario.isScenarioFinished()) {
      this.isCompleted = true;
      this.isRunning = false;
      this.addLog('Simulation complete   All hazards successfully avoided with zero collisions', 'system');
      const finalTelemetry = this.computeTelemetry(risk, decision.statusText);
      this.onCompletion?.(finalTelemetry);
    }

    this.broadcastTelemetry(risk, decision.statusText);
  }

  private computeTelemetry(risk?: any, actionText?: string): TelemetryData {
    const hazards = this.scenario.getHazards();
    const perceived = this.perception.process(this.ego, hazards);
    const r = risk || this.riskAssessor.assess(this.ego, perceived);

    let vehicleStatus: TelemetryData['vehicleStatus'] = 'NORMAL';
    const state = this.decisionEngine.getCurrentState();
    if (state.includes('STEER') || state === 'LANE_CHANGE') {
      vehicleStatus = 'AVOIDING';
    } else if (state === 'STOP' || state === 'RESUME') {
      vehicleStatus = 'SAFE';
    }

    let detectionStatus: TelemetryData['detectionStatus'] = 'Scanning';
    if (r.primaryHazard) {
      detectionStatus = r.isThreat ? 'Hazard Detected' : 'Scanning';
    } else if (this.simTime > 27.0) {
      detectionStatus = 'Path Clear';
    }

    const primary = r.primaryHazard;
    const hazardDist = primary ? Math.max(0, primary.y - this.ego.y) : null;
    const ttc = primary && this.ego.speed > 0.5 ? hazardDist! / this.ego.speed : null;

    if (this.activeBanner && this.simTime >= this.activeBanner.expireAt) {
      this.activeBanner = null;
    }

    return {
      simTime: this.simTime,
      speedKmh: Math.round(this.ego.speed * 3.6),
      currentHazard: primary ? primary.type : 'None',
      hazardDistance: hazardDist !== null ? Math.round(hazardDist) : null,
      detectionStatus,
      riskLevel: r.level,
      riskScore: r.score,
      currentAction: actionText || 'Cruising safely',
      vehicleStatus,
      collisions: this.safetySupervisor.getCollisions(),
      stoppingDistance: Number(r.stoppingDistance.toFixed(1)),
      ttc: ttc !== null ? Number(ttc.toFixed(1)) : null,
      currentLane: this.ego.currentLane,
      minClearanceOverall: Number(this.safetySupervisor.getMinClearance().toFixed(2)),
      distanceTraveledMeters: Math.round(this.ego.y),
      activeBanner: this.activeBanner,
    };
  }

  private broadcastTelemetry(risk?: any, actionText?: string): void {
    const t = this.computeTelemetry(risk, actionText);
    this.onTelemetryUpdate?.(t);
  }

  /**
   * Main Render function called inside requestAnimationFrame
   */
  public render(ctx: CanvasRenderingContext2D, width: number, height: number): void {
    ctx.clearRect(0, 0, width, height);
    const hazards = this.scenario.getHazards();

    if (this.settings.viewMode === 'cockpit_3d') {
      // 1. Realistic 3D Perspective Driver/Windshield Driving Environment
      this.environment3D.render(
        ctx,
        width,
        height,
        this.ego,
        hazards,
        this.simTime,
        this.settings
      );
      return;
    }

    // 2. Top-down tactical engineering view fallback
    this.roadRenderer.renderRoad(ctx, this.camX, this.camY, width, height, this.simTime);

    for (const h of hazards) {
      if (h.state === 'cleared') continue;
      const sx = width * 0.5 + (h.x - this.camX) * 18;
      const sy = height * 0.70 - (h.y - this.camY) * 18;
      if (sy < -120 || sy > height + 120) continue;

      if (h.type === 'pothole') {
        this.potholeRenderer.renderPothole(ctx, sx, sy);
      } else if (h.type === 'animal') {
        this.hazardSprites.renderCow(ctx, sx, sy, h);
      } else if (h.type === 'pedestrian') {
        this.hazardSprites.renderPedestrian(ctx, sx, sy, h);
      } else if (h.type === 'vehicle') {
        this.hazardSprites.renderVehicleAhead(ctx, sx, sy, h);
      }

      if (this.settings.showTelemetryOverlay && h.y > this.ego.y - 2.0 && h.y < this.ego.y + 60.0) {
        this.drawHazardBracket(ctx, sx, sy, h);
      }
    }

    const egoSx = width * 0.5 + (this.ego.x - this.camX) * 18;
    const egoSy = height * 0.70 - (this.ego.y - this.camY) * 18;

    if (this.settings.showSensorCone) {
      this.drawSensorCone(ctx, egoSx, egoSy);
    }

    if (this.settings.showPlannedPath) {
      this.drawPlannedTrajectory(ctx, width, height);
    }

    this.vehicleSprite.renderVehicle(ctx, egoSx, egoSy, this.ego, this.simTime);
  }

  private drawSensorCone(ctx: CanvasRenderingContext2D, sx: number, sy: number): void {
    ctx.save();
    ctx.translate(sx, sy);
    ctx.rotate(this.ego.heading);

    const rangePx = 65 * 18;
    const halfFov = (38 * Math.PI) / 180;

    const coneGrad = ctx.createRadialGradient(0, 0, 10, 0, 0, rangePx);
    coneGrad.addColorStop(0, 'rgba(56, 189, 248, 0.22)');
    coneGrad.addColorStop(0.35, 'rgba(14, 165, 233, 0.08)');
    coneGrad.addColorStop(0.85, 'rgba(14, 165, 233, 0.02)');
    coneGrad.addColorStop(1, 'rgba(14, 165, 233, 0.0)');
    ctx.fillStyle = coneGrad;

    ctx.beginPath();
    ctx.moveTo(0, 0);
    ctx.arc(0, 0, rangePx, -Math.PI / 2 - halfFov, -Math.PI / 2 + halfFov);
    ctx.closePath();
    ctx.fill();

    // Laser boundary lines
    ctx.strokeStyle = 'rgba(56, 189, 248, 0.45)';
    ctx.lineWidth = 1.2;
    ctx.setLineDash([4, 6]);
    ctx.lineDashOffset = -this.simTime * 18;
    ctx.stroke();

    ctx.restore();
  }

  private drawPlannedTrajectory(
    ctx: CanvasRenderingContext2D,
    canvasW: number,
    canvasH: number
  ): void {
    ctx.save();
    const targetX = LANE_CENTERS[this.ego.targetLane];
    const pts: { x: number; y: number }[] = [];

    for (let dy = 0; dy < 38; dy += 2) {
      const worldY = this.ego.y + dy;
      const blend = Math.min(1, dy / 18.0);
      const worldX = this.ego.x + (targetX - this.ego.x) * blend;
      const sx = canvasW * 0.5 + (worldX - this.camX) * 18;
      const sy = canvasH * 0.70 - (worldY - this.camY) * 18;
      pts.push({ x: sx, y: sy });
    }

    if (pts.length > 1) {
      ctx.strokeStyle = 'rgba(34, 197, 94, 0.25)';
      ctx.lineWidth = 7.0;
      ctx.beginPath();
      ctx.moveTo(pts[0].x, pts[0].y);
      for (let i = 1; i < pts.length; i++) ctx.lineTo(pts[i].x, pts[i].y);
      ctx.stroke();

      ctx.strokeStyle = '#22c55e';
      ctx.lineWidth = 2.5;
      ctx.setLineDash([8, 6]);
      ctx.lineDashOffset = -this.simTime * 28.0;
      ctx.beginPath();
      ctx.moveTo(pts[0].x, pts[0].y);
      for (let i = 1; i < pts.length; i++) ctx.lineTo(pts[i].x, pts[i].y);
      ctx.stroke();
    }
    ctx.restore();
  }

  private drawHazardBracket(
    ctx: CanvasRenderingContext2D,
    sx: number,
    sy: number,
    h: HazardObject
  ): void {
    const boxW = Math.max(32, h.width * 18 + 12);
    const boxH = Math.max(32, h.length * 18 + 12);
    const dist = Math.round(h.y - this.ego.y);

    ctx.save();
    ctx.strokeStyle = dist < 20 ? 'rgba(239, 68, 68, 0.90)' : 'rgba(56, 189, 248, 0.85)';
    ctx.lineWidth = 1.4;
    const bLen = 6;
    const left = sx - boxW / 2;
    const right = sx + boxW / 2;
    const top = sy - boxH / 2;
    const bottom = sy + boxH / 2;

    ctx.beginPath();
    ctx.moveTo(left, top + bLen);
    ctx.lineTo(left, top);
    ctx.lineTo(left + bLen, top);
    ctx.moveTo(right - bLen, top);
    ctx.lineTo(right, top);
    ctx.lineTo(right, top + bLen);
    ctx.moveTo(left, bottom - bLen);
    ctx.lineTo(left, bottom);
    ctx.lineTo(left + bLen, bottom);
    ctx.moveTo(right - bLen, bottom);
    ctx.lineTo(right, bottom);
    ctx.lineTo(right, bottom - bLen);
    ctx.stroke();

    ctx.fillStyle = 'rgba(2, 6, 23, 0.90)';
    ctx.fillRect(left, top - 18, 88, 15);
    ctx.fillStyle = dist < 20 ? '#ef4444' : '#38bdf8';
    ctx.font = '9px "JetBrains Mono", monospace';
    const label = h.type === 'vehicle' ? 'AUTO' : h.type.toUpperCase();
    ctx.fillText(`${label} ${dist}m`, left + 4, top - 6);
    ctx.restore();
  }

  public getEgo(): EgoVehicleState {
    return this.ego;
  }

  public getSimTime(): number {
    return this.simTime;
  }

  public isSimRunning(): boolean {
    return this.isRunning;
  }

  public isSimPaused(): boolean {
    return this.isPaused;
  }

  public getActiveHazards(): HazardObject[] {
    return this.scenario.getHazards();
  }
}
