export type FrameSource =
  | 'schematic-workbench'
  | 'predicted-workbench'
  | 'firmware-led-stream'
  | 'serial-capture'
  | 'imported-capture';

export type RuleCheckLevel = 'L0' | 'L1' | 'L2' | 'L3' | 'L4' | 'L5';

export type LedFrame = {
  source: FrameSource;
  timestampMs: number;
  leds: Uint8Array;
  metadata: Record<string, unknown>;
};

export type DesignIntent = {
  effectIdentity: string;
  dominantMusicalEvent: string;
  silenceBehaviour: string;
  forbiddenForThisEffect: string[];
};

export type VisualStory = {
  sentence1: string;
  sentence2: string;
};

export type AudioSignalSelection = {
  dominant: string;
  modifiers: string[];
};

export type VisualPrimitiveSelection = {
  primitive: string;
  motionRule: string;
  decayRule: string;
  flags: string[];
};

export type RuleSeverity = 'blocked' | 'warning' | 'info';

export type RuleFinding = {
  id: string;
  severity: RuleSeverity;
  message: string;
  evidence: string;
};

export type RulesAudit = {
  level: RuleCheckLevel;
  findings: RuleFinding[];
  blocked: boolean;
};

export type ActualCapture = {
  name: string;
  source: FrameSource;
  frameCount: number;
  frameSize: number;
  importedAt: string;
};

export type RepairSession = {
  effectId: string;
  effectName: string;
  sourceFiles: string[];
  createdAt: string;
  designIntent: DesignIntent;
  visualStory: VisualStory;
  selectedAudio: AudioSignalSelection;
  selectedVisual: VisualPrimitiveSelection;
  rulesAudit: RulesAudit;
  actualCaptures: ActualCapture[];
};

export type CatalogueStatus =
  | 'supported'
  | 'blocked'
  | 'researchOnly'
  | 'transportOnly'
  | 'sourceConfirmed'
  | 'firmwareOnly'
  | 'yamlDocumented'
  | 'compileGated'
  | 'invariantBlocked';

export type CatalogueEntry = {
  key: string;
  label: string;
  status: CatalogueStatus;
  source: string;
  rationale: string;
};
