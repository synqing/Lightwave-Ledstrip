import { audioSignalCatalogue } from '../catalogues/audio.manual';
import { transportAuthorityNote, transportCatalogue } from '../catalogues/transport.manual';
import { visualPrimitiveCatalogue } from '../catalogues/vp.manual';
import { auditRules } from '../catalogues/rules';
import { importCaptureBytes } from '../capture/importCapture';
import { makeCentrePulseFrame } from '../capture/frameParser';
import { generateDirective } from '../directive/generateDirective';
import { ByteStripRenderer } from '../renderers/ByteStripRenderer';
import type { ActualCapture, CatalogueEntry, DesignIntent, LedFrame, RepairSession, VisualStory } from '../types';

type CatalogueTab = 'audio' | 'visual' | 'transport';

type AppState = {
  designIntent: DesignIntent;
  visualStory: VisualStory;
  dominantAudio: string;
  visualPrimitive: string;
  visualFlags: string[];
  frames: LedFrame[];
  captures: ActualCapture[];
  catalogueTab: CatalogueTab;
};

const dominantAudioKeys = new Set([
  'onset.kick.fired',
  'onset.snare.fired',
  'onset.hihat.fired',
  'onset.transient.fired',
]);

const state: AppState = {
  designIntent: {
    effectIdentity: 'Ephemeral Fresnel lens consequences, not a scanning carrier.',
    dominantMusicalEvent: 'Reliable kick onset.',
    silenceBehaviour: 'Dark in silence.',
    forbiddenForThisEffect: ['pendulum focus motion', 'ambient carrier', 'ctx.gHue hue drift', 'RMS speed modulation'],
  },
  visualStory: {
    sentence1: 'Each reliable kick creates a bright Fresnel lens at the centre pair.',
    sentence2: 'The lens travels outward through fading rings and carries the chord colour captured at spawn.',
  },
  dominantAudio: 'onset.kick.fired',
  visualPrimitive: 'centre-pulse',
  visualFlags: [],
  frames: [makeCentrePulseFrame()],
  captures: [],
  catalogueTab: 'audio',
};

function schematicColourForAudio(key: string): [number, number, number] {
  if (key === 'onset.snare.fired') return [69, 214, 210];
  if (key === 'onset.hihat.fired') return [113, 217, 139];
  if (key === 'onset.transient.fired') return [255, 111, 89];
  return [255, 184, 77];
}

function displayFrame(): LedFrame | null {
  if (state.captures.length > 0) return state.frames[0] ?? null;
  return makeCentrePulseFrame(schematicColourForAudio(state.dominantAudio));
}

function createSession(): RepairSession {
  const selectedVisual = {
    primitive: state.visualPrimitive,
    motionRule: 'centre 79/80 outward',
    decayRule: 'monotonic event consequence decay',
    flags: state.visualFlags,
  };
  const selectedAudio = {
    dominant: state.dominantAudio,
    modifiers: ['chroma'],
  };
  const rulesAudit = auditRules({
    designIntent: state.designIntent,
    visualStory: state.visualStory,
    selectedAudio,
    selectedVisual,
  });

  return {
    effectId: '0x1B04',
    effectName: 'Fresnel Caustic Sweep',
    sourceFiles: ['firmware-v3/src/effects/ieffect/LGPFresnelCausticSweepEffect.cpp'],
    createdAt: new Date().toISOString(),
    designIntent: state.designIntent,
    visualStory: state.visualStory,
    selectedAudio,
    selectedVisual,
    rulesAudit,
    actualCaptures: state.captures,
  };
}

function byKey(entries: CatalogueEntry[], key: string): CatalogueEntry | undefined {
  return entries.find((entry) => entry.key === key);
}

function element<K extends keyof HTMLElementTagNameMap>(
  tag: K,
  className?: string,
  text?: string,
): HTMLElementTagNameMap[K] {
  const node = document.createElement(tag);
  if (className) node.className = className;
  if (text !== undefined) node.textContent = text;
  return node;
}

function pill(label: string, tone: 'ok' | 'warn' | 'blocked' | 'neutral' = 'neutral'): HTMLElement {
  return element('span', `status-pill ${tone}`, label);
}

function metric(label: string, value: string, tone: 'ok' | 'warn' | 'blocked' | 'neutral' = 'neutral'): HTMLElement {
  const item = element('div', `metric ${tone}`);
  item.append(element('span', 'metric-label', label), element('strong', undefined, value));
  return item;
}

function sectionShell(title: string, eyebrow: string): HTMLElement {
  const section = element('section', 'panel');
  const header = element('div', 'panel-header');
  header.append(element('span', 'eyebrow', eyebrow), element('h2', undefined, title));
  section.append(header);
  return section;
}

function textField(label: string, value: string, rows: number, onInput: (value: string) => void): HTMLElement {
  const wrapper = element('label', 'field');
  wrapper.append(element('span', undefined, label));
  const input = document.createElement('textarea');
  input.value = value;
  input.rows = rows;
  input.addEventListener('input', () => {
    onInput(input.value);
    render();
  });
  wrapper.append(input);
  return wrapper;
}

function selectField(
  label: string,
  value: string,
  values: CatalogueEntry[],
  onChange: (value: string) => void,
): HTMLElement {
  const wrapper = element('label', 'field');
  wrapper.append(element('span', undefined, label));
  const select = document.createElement('select');
  for (const item of values) {
    const option = document.createElement('option');
    option.value = item.key;
    option.textContent = item.label;
    option.selected = item.key === value;
    select.append(option);
  }
  select.addEventListener('change', () => {
    onChange(select.value);
    render();
  });
  wrapper.append(select);
  return wrapper;
}

function decisionRail(session: RepairSession): HTMLElement {
  const rail = element('aside', 'rail');
  const brand = element('div', 'brand-block');
  brand.append(element('span', 'brand-kicker', 'K1 LightwaveOS'), element('strong', undefined, 'Effect Repair Workbench'));

  const steps = [
    ['Intent', 'Visual identity and silence law', 'done'],
    ['Story', 'Two sentence behaviour contract', 'done'],
    ['Rules', session.rulesAudit.blocked ? 'Blocked choices need repair' : 'Current choices pass L1', session.rulesAudit.blocked ? 'blocked' : 'done'],
    ['Capture', state.captures.length ? 'Actual bytes loaded' : 'Awaiting hardware bytes', state.captures.length ? 'done' : 'waiting'],
    ['Directive', 'Agent-consumable repair brief', 'waiting'],
  ];

  const list = element('ol', 'step-list');
  for (const [title, detail, status] of steps) {
    const item = element('li', `step ${status}`);
    item.append(element('strong', undefined, title), element('span', undefined, detail));
    list.append(item);
  }

  const guidance = element('div', 'guidance');
  guidance.append(
    element('span', 'eyebrow', 'Operating mode'),
    element('p', undefined, 'Track B is honest capture-only work. It helps you author and reject directives before firmware work, then imports actual LED bytes after flash.'),
  );

  rail.append(brand, list, guidance);
  return rail;
}

function stagePanel(session: RepairSession): HTMLElement {
  const stage = element('section', 'stage');
  const header = element('div', 'stage-header');
  const title = element('div');
  title.append(element('span', 'eyebrow', 'Byte-strip surface'), element('h1', undefined, '0x1B04 Fresnel Caustic Sweep'));
  const status = element('div', 'status-row');
  status.append(
    pill('Track B', 'warn'),
    pill(session.rulesAudit.blocked ? 'Rules blocked' : 'L1 clear', session.rulesAudit.blocked ? 'blocked' : 'ok'),
    pill(state.captures.length ? 'Hardware bytes loaded' : 'No hardware truth', state.captures.length ? 'ok' : 'warn'),
  );
  header.append(title, status);

  const canvas = document.createElement('canvas');
  canvas.className = 'strip-canvas';
  const canvasShell = element('div', 'canvas-shell');
  canvasShell.append(canvas);

  const renderer = new ByteStripRenderer(canvas);
  const frame = displayFrame();
  renderer.render(frame);

  const capture = state.captures[0];
  const stats = element('div', 'stage-metrics');
  stats.append(
    metric('Frame source', frame?.source ?? 'none', frame?.source === 'predicted-workbench' ? 'warn' : 'ok'),
    metric('Frames loaded', String(state.frames.length), state.captures.length ? 'ok' : 'neutral'),
    metric('Capture size', capture ? `${capture.frameSize} bytes` : 'none', capture ? 'ok' : 'warn'),
    metric('Origin rule', '79/80 outward', 'ok'),
  );

  const story = element('div', 'story-strip');
  story.append(element('p', undefined, state.visualStory.sentence1), element('p', undefined, state.visualStory.sentence2));

  stage.append(header, canvasShell, stats, story);
  return stage;
}

function flagControls(): HTMLElement {
  const flags = [
    ['reversingPosition', 'Reversing position'],
    ['ambientCarrier', 'Ambient carrier'],
    ['ctx.gHue', 'ctx.gHue drift'],
    ['linearSweep', 'Linear sweep'],
    ['rainbow', 'Rainbow'],
  ];
  const wrapper = element('div', 'flag-grid');
  for (const [flag, label] of flags) {
    const item = element('label', 'check');
    const input = document.createElement('input');
    input.type = 'checkbox';
    input.checked = state.visualFlags.includes(flag);
    input.addEventListener('change', () => {
      state.visualFlags = input.checked
        ? [...state.visualFlags, flag]
        : state.visualFlags.filter((value) => value !== flag);
      render();
    });
    item.append(input, element('span', undefined, label));
    wrapper.append(item);
  }
  return wrapper;
}

function inspectorPanel(): HTMLElement {
  const inspector = element('aside', 'inspector');
  const intent = sectionShell('Design intent', 'Captain input');
  intent.append(
    textField('Visual identity', state.designIntent.effectIdentity, 2, (value) => (state.designIntent.effectIdentity = value)),
    textField('Dominant musical event', state.designIntent.dominantMusicalEvent, 1, (value) => (state.designIntent.dominantMusicalEvent = value)),
    textField('Silence behaviour', state.designIntent.silenceBehaviour, 1, (value) => (state.designIntent.silenceBehaviour = value)),
  );

  const story = sectionShell('Visual story', 'Two-sentence gate');
  story.append(
    textField('Sentence 1', state.visualStory.sentence1, 2, (value) => (state.visualStory.sentence1 = value)),
    textField('Sentence 2', state.visualStory.sentence2, 2, (value) => (state.visualStory.sentence2 = value)),
  );

  const wiring = sectionShell('AP / VP wiring', 'Allowed surface');
  const audioOptions = audioSignalCatalogue.filter((entry) => dominantAudioKeys.has(entry.key));
  wiring.append(
    selectField('Dominant audio event', state.dominantAudio, audioOptions, (value) => (state.dominantAudio = value)),
    selectField('Visual primitive', state.visualPrimitive, visualPrimitiveCatalogue, (value) => (state.visualPrimitive = value)),
    entrySummary(byKey(audioSignalCatalogue, state.dominantAudio)),
    entrySummary(byKey(visualPrimitiveCatalogue, state.visualPrimitive)),
  );

  const probes = sectionShell('Failure probes', 'Block deliberately');
  probes.append(flagControls());

  const capture = capturePanel();
  inspector.append(intent, story, wiring, probes, capture);
  return inspector;
}

function entrySummary(entry: CatalogueEntry | undefined): HTMLElement {
  const box = element('div', 'entry-summary');
  if (!entry) {
    box.append(element('span', 'muted', 'No catalogue entry selected.'));
    return box;
  }
  box.append(pill(entry.status, entry.status === 'supported' ? 'ok' : entry.status === 'blocked' ? 'blocked' : 'warn'));
  box.append(element('strong', undefined, entry.key), element('p', undefined, entry.rationale), element('small', undefined, entry.source));
  return box;
}

function capturePanel(): HTMLElement {
  const capture = sectionShell('Capture import', 'Hardware evidence');
  const input = document.createElement('input');
  input.type = 'file';
  input.accept = '.bin,application/octet-stream';
  input.addEventListener('change', async () => {
    const captureFile = input.files?.[0];
    if (!captureFile) return;
    const bytes = new Uint8Array(await captureFile.arrayBuffer());
    const imported = importCaptureBytes(captureFile.name, bytes, 'imported-capture');
    state.frames = imported.frames;
    state.captures = [imported.capture];
    render();
  });
  const button = element('label', 'file-button', 'Import .bin capture');
  button.append(input);
  const note = element('p', 'panel-note', state.captures.length
    ? `${state.captures[0].name}: ${state.captures[0].frameCount} frame(s), ${state.captures[0].frameSize} bytes each.`
    : 'No capture loaded. The current strip image is an event-colour schematic centre-origin check, not hardware truth.');
  capture.append(button, note);
  return capture;
}

function rulesPanel(session: RepairSession): HTMLElement {
  const panel = sectionShell('Rules audit', session.rulesAudit.level);
  panel.classList.add(session.rulesAudit.blocked ? 'panel-blocked' : 'panel-clear');
  const findings = element('div', 'finding-list');
  if (session.rulesAudit.findings.length === 0) {
    const clear = element('div', 'finding clear');
    clear.append(element('strong', undefined, 'No blockers'), element('span', undefined, 'The current AP/VP pairing passes the encoded 0x1B04 repair rules.'));
    findings.append(clear);
  } else {
    for (const finding of session.rulesAudit.findings) {
      const row = element('div', `finding ${finding.severity}`);
      row.append(element('strong', undefined, finding.id), element('span', undefined, finding.message), element('small', undefined, finding.evidence));
      findings.append(row);
    }
  }
  panel.append(findings);
  return panel;
}

function cataloguePanel(): HTMLElement {
  const panel = sectionShell('Catalogue intelligence', 'Source reconciliation');
  const tabs = element('div', 'tabs');
  const tabLabels: Array<[CatalogueTab, string]> = [
    ['audio', 'Audio signals'],
    ['visual', 'Visual primitives'],
    ['transport', 'Transport'],
  ];
  for (const [key, label] of tabLabels) {
    const tab = document.createElement('button');
    tab.type = 'button';
    tab.className = key === state.catalogueTab ? 'tab active' : 'tab';
    tab.textContent = label;
    tab.addEventListener('click', () => {
      state.catalogueTab = key;
      render();
    });
    tabs.append(tab);
  }

  const entries =
    state.catalogueTab === 'audio'
      ? audioSignalCatalogue
      : state.catalogueTab === 'visual'
        ? visualPrimitiveCatalogue
        : transportCatalogue;

  const list = element('div', 'catalogue-list');
  for (const entry of entries) {
    const row = element('article', `catalogue-row ${entry.status}`);
    row.append(
      element('strong', undefined, entry.label),
      element('span', undefined, entry.key),
      element('p', undefined, entry.rationale),
      element('small', undefined, entry.source),
    );
    list.append(row);
  }

  panel.append(tabs, list);
  if (state.catalogueTab === 'transport') {
    panel.append(element('p', 'authority', transportAuthorityNote));
  }
  return panel;
}

function directivePanel(session: RepairSession): HTMLElement {
  const panel = sectionShell('Generated directive', 'Agent handoff artefact');
  const directive = document.createElement('textarea');
  directive.className = 'directive';
  directive.readOnly = true;
  directive.value = generateDirective(session);
  panel.append(directive);
  return panel;
}

function render(): void {
  const app = document.querySelector<HTMLDivElement>('#app');
  if (!app) return;
  app.innerHTML = '';

  const session = createSession();
  const shell = element('main', 'workbench');
  const top = element('header', 'topbar');
  const title = element('div', 'topbar-title');
  title.append(element('span', 'eyebrow', 'Repair cockpit'), element('strong', undefined, `${session.effectId} / ${session.effectName}`));
  const topStatus = element('div', 'status-row');
  topStatus.append(
    pill('Centre origin locked', 'ok'),
    pill('No rainbows', 'ok'),
    pill('Capture-only v0', 'warn'),
  );
  top.append(title, topStatus);

  const lower = element('section', 'evidence-deck');
  lower.append(rulesPanel(session), cataloguePanel(), directivePanel(session));

  shell.append(top, decisionRail(session), stagePanel(session), inspectorPanel(), lower);
  app.append(shell);
}

export function mountApp(): void {
  render();
}
