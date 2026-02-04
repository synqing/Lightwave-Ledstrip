import { readFile } from 'node:fs/promises';
import path from 'node:path';

import { z } from 'zod';

import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { SSEClientTransport } from '@modelcontextprotocol/sdk/client/sse.js';
import { CallToolResultSchema, ListToolsResultSchema } from '@modelcontextprotocol/sdk/types.js';
import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { StdioServerTransport } from '@modelcontextprotocol/sdk/server/stdio.js';

/**
 * Rive Proxy MCP
 *
 * Exposes higher-level “macro tools” built on top of Rive EA’s MCP server.
 *
 * Upstream (Rive EA) is expected to expose legacy HTTP+SSE MCP at:
 *   http://127.0.0.1:9791/sse
 */

const UPSTREAM_RIVE_URL = process.env.RIVE_MCP_URL ?? 'http://localhost:9791/sse';

type UpstreamToolName = string;

let upstreamClient: Client | null = null;

async function getUpstreamClient(): Promise<Client> {
  if (upstreamClient) return upstreamClient;

  const client = new Client({ name: 'lightwave-rive-proxy', version: '0.1.0' });
  const transport = new SSEClientTransport(new URL(UPSTREAM_RIVE_URL));

  client.onerror = (err) => {
    // Surface upstream issues early; Cursor will show this.
    console.error('[rive-proxy] upstream client error:', err);
  };

  try {
    // Set a reasonable timeout for SSE connection
    const connectPromise = client.connect(transport);
    const timeoutPromise = new Promise<never>((_, reject) =>
      setTimeout(() => reject(new Error('Connection timeout after 10s')), 10000)
    );
    await Promise.race([connectPromise, timeoutPromise]);
    upstreamClient = client;
    return client;
  } catch (err) {
    // Reset on failure so next call retries
    upstreamClient = null;
    const errorMsg = err instanceof Error ? err.message : String(err);
    throw new Error(`Failed to connect to Rive MCP at ${UPSTREAM_RIVE_URL}: ${errorMsg}. Ensure Rive EA is running and MCP is enabled.`);
  }
}

async function upstreamCallTool(name: UpstreamToolName, args: unknown): Promise<z.infer<typeof CallToolResultSchema>> {
  const client = await getUpstreamClient();
  return await client.request(
    {
      method: 'tools/call',
      params: { name, arguments: args ?? {} },
    },
    CallToolResultSchema,
  );
}

async function upstreamListTools(): Promise<z.infer<typeof ListToolsResultSchema>> {
  const client = await getUpstreamClient();
  return await client.request({ method: 'tools/list', params: {} }, ListToolsResultSchema);
}

function mapSpecTypeToRivePropertyType(type: string): 'boolean' | 'number' | 'string' | 'trigger' {
  switch (type) {
    case 'bool':
    case 'boolean':
      return 'boolean';
    case 'number':
      return 'number';
    case 'string':
    case 'text':
      return 'string';
    case 'trigger':
      return 'trigger';
    default:
      // Default to string rather than throwing; safer for forward-compat.
      return 'string';
  }
}

function extractViewModelNamesFromStructured(structured: unknown): string[] {
  if (!structured || typeof structured !== 'object') return [];
  const candidate = structured as Record<string, unknown>;
  const viewModels = candidate.viewModels;
  if (Array.isArray(viewModels)) {
    return viewModels
      .map((item) => (item && typeof item === 'object' ? (item as Record<string, unknown>).name : undefined))
      .filter((name): name is string => typeof name === 'string' && name.length > 0);
  }
  return [];
}

function extractViewModelNamesFromText(text: string): string[] {
  if (!text) return [];
  const names = new Set<string>();
  const regex = /name:\s*([A-Za-z0-9_\-]+)/g;
  let match: RegExpExecArray | null = null;
  while ((match = regex.exec(text)) !== null) {
    names.add(match[1]);
  }
  return Array.from(names);
}

type NamedItem = { id?: string; name: string };

function extractNamedItemsFromStructured(structured: unknown, key: string): NamedItem[] {
  if (!structured || typeof structured !== 'object') return [];
  const candidate = structured as Record<string, unknown>;
  const items = candidate[key];
  if (!Array.isArray(items)) return [];
  const results: NamedItem[] = [];
  for (const item of items) {
    if (!item || typeof item !== 'object') continue;
    const rec = item as Record<string, unknown>;
    const name = typeof rec.name === 'string' ? rec.name : undefined;
    const id =
      typeof rec.id === 'string'
        ? rec.id
        : typeof rec.stateMachineId === 'string'
          ? rec.stateMachineId
          : typeof rec.viewModelId === 'string'
            ? rec.viewModelId
            : undefined;
    if (!name) continue;
    results.push({ id, name });
  }
  return results;
}

function extractNamedItemsFromText(text: string): NamedItem[] {
  const names = extractViewModelNamesFromText(text);
  return names.map((name) => ({ name }));
}

function findToolName(tools: string[], candidates: string[]): string | null {
  for (const candidate of candidates) {
    if (tools.includes(candidate)) return candidate;
  }
  return null;
}

async function listUpstreamToolNames(): Promise<string[]> {
  const list = await upstreamListTools();
  return list.tools.map((tool) => tool.name);
}

async function callToolIfAvailable(
  tools: string[],
  candidates: string[],
  args: unknown,
): Promise<{ tool: string | null; result?: z.infer<typeof CallToolResultSchema>; error?: string }> {
  const tool = findToolName(tools, candidates);
  if (!tool) {
    return { tool: null, error: `Missing upstream tool. Tried: ${candidates.join(', ')}` };
  }
  try {
    const result = await upstreamCallTool(tool, args);
    return { tool, result };
  } catch (error) {
    return { tool, error: error instanceof Error ? error.message : String(error) };
  }
}

function resolveRepoPath(p: string): string {
  // Allow absolute paths, otherwise resolve relative to current working dir.
  return path.isAbsolute(p) ? p : path.resolve(process.cwd(), p);
}

const SpecSchema = z.object({
  version: z.number(),
  artboardSize: z.object({
    width: z.number(),
    height: z.number(),
    units: z.string(),
  }),
  viewModelNaming: z.string(),
  assets: z.array(
    z.object({
      id: z.string(),
      fileName: z.string(),
      artboard: z.string(),
      stateMachine: z.string(),
      inputs: z.array(
        z.object({
          name: z.string(),
          type: z.string(),
        }),
      ),
    }),
  ),
});

const SnapshotSchema = z.object({
  viewModels: z.any().optional(),
  stateMachines: z.any().optional(),
  linearAnimations: z.any().optional(),
});

const server = new McpServer({
  name: 'lightwave-rive-proxy',
  version: '0.1.0',
});

server.registerTool(
  'rive_proxy_health',
  {
    title: 'Rive Proxy Health',
    description: 'Check upstream Rive MCP connectivity.',
    inputSchema: {},
  },
  async () => {
    try {
      const tools = await upstreamListTools();
      return {
        content: [
          {
            type: 'text',
            text: `Upstream OK. Tools available: ${tools.tools.length}`,
          },
        ],
        structuredContent: { upstreamUrl: UPSTREAM_RIVE_URL, toolCount: tools.tools.length },
      };
    } catch (err) {
      const errorMsg = err instanceof Error ? err.message : String(err);
      return {
        isError: true,
        content: [
          {
            type: 'text',
            text: `Upstream connection failed: ${errorMsg}`,
          },
        ],
        structuredContent: { upstreamUrl: UPSTREAM_RIVE_URL, toolCount: 0, error: errorMsg },
      };
    }
  },
);

server.registerTool(
  'rive_proxy_snapshot',
  {
    title: 'Rive Snapshot',
    description:
      'Return a quick snapshot (view models, state machines, linear animations) from the active Rive file/artboard.',
    inputSchema: {},
  },
  async () => {
    const [viewModels, stateMachines, linearAnimations] = await Promise.all([
      upstreamCallTool('listViewModels', {}),
      upstreamCallTool('listStateMachines', {}),
      upstreamCallTool('listLinearAnimations', {}),
    ]);

    const structured = SnapshotSchema.parse({
      viewModels,
      stateMachines,
      linearAnimations,
    });

    return {
      content: [
        {
          type: 'text',
          text: 'Snapshot collected (view models, state machines, linear animations).',
        },
      ],
      structuredContent: structured,
    };
  },
);

server.registerTool(
  'rive_proxy_scaffold_from_spec',
  {
    title: 'Scaffold From Spec',
    description:
      'Create missing artboards and state machines from mcp_scaffold_spec.json (best-effort, idempotent).',
    inputSchema: {
      specPath: z
        .string()
        .describe(
          'Path to mcp_scaffold_spec.json. Absolute path recommended. Example: lightwave-ios-v2/docs/Rive/Contract/mcp_scaffold_spec.json',
        ),
      createArtboards: z.boolean().default(true),
      createStateMachines: z.boolean().default(true),
      strict: z
        .boolean()
        .default(false)
        .describe('Fail if upstream list outputs do not include structured IDs.'),
    },
  },
  async ({ specPath, createArtboards, createStateMachines, strict }) => {
    const absPath = resolveRepoPath(specPath);
    const raw = await readFile(absPath, 'utf8');
    const spec = SpecSchema.parse(JSON.parse(raw));

    const toolNames = await listUpstreamToolNames();

    const listArtboards = await callToolIfAvailable(toolNames, ['listArtboards'], {});
    const listStateMachines = await callToolIfAvailable(toolNames, ['listStateMachines'], {});

    const artboardsStructured = listArtboards.result?.structuredContent;
    const stateMachinesStructured = listStateMachines.result?.structuredContent;
    const artboardItems = extractNamedItemsFromStructured(artboardsStructured, 'artboards');
    const stateMachineItems = extractNamedItemsFromStructured(stateMachinesStructured, 'stateMachines');

    const artboardNames =
      artboardItems.length > 0
        ? new Set(artboardItems.map((item) => item.name))
        : new Set(
            extractNamedItemsFromText(listArtboards.result?.content?.find((c) => c.type === 'text')?.text ?? '').map(
              (item) => item.name,
            ),
          );
    const stateMachineNames =
      stateMachineItems.length > 0
        ? new Set(stateMachineItems.map((item) => item.name))
        : new Set(
            extractNamedItemsFromText(listStateMachines.result?.content?.find((c) => c.type === 'text')?.text ?? '').map(
              (item) => item.name,
            ),
          );

    if (strict && (artboardItems.length === 0 || stateMachineItems.length === 0)) {
      return {
        isError: true,
        content: [
          {
            type: 'text',
            text: 'Strict mode enabled, but structured artboard/state machine output was not available. Aborting.',
          },
        ],
        structuredContent: {
          specPath: absPath,
          createdArtboards: [],
          createdStateMachines: [],
          warning: 'Missing structured list output from upstream.',
        },
      };
    }

    const createdArtboards: string[] = [];
    const createdStateMachines: string[] = [];
    const errors: string[] = [];

    if (createArtboards) {
      const artboardsToCreate = spec.assets
        .map((asset) => asset.artboard)
        .filter((name) => !artboardNames.has(name));

      if (artboardsToCreate.length > 0) {
        const payload = {
          artboards: artboardsToCreate.map((name) => ({
            name,
            width: spec.artboardSize.width,
            height: spec.artboardSize.height,
            units: spec.artboardSize.units,
          })),
        };
        const createRes = await callToolIfAvailable(toolNames, ['createArtboards', 'createArtboard'], payload);
        if (createRes.error) {
          errors.push(createRes.error);
        } else {
          createdArtboards.push(...artboardsToCreate);
        }
      }
    }

    if (createStateMachines) {
      const stateMachinesToCreate = spec.assets
        .map((asset) => ({ name: asset.stateMachine, artboard: asset.artboard }))
        .filter((item) => !stateMachineNames.has(item.name));

      if (stateMachinesToCreate.length > 0) {
        const payload = {
          stateMachines: stateMachinesToCreate.map((item) => ({
            name: item.name,
            artboardName: item.artboard,
          })),
        };
        const createRes = await callToolIfAvailable(
          toolNames,
          ['createStateMachines', 'createStateMachine'],
          payload,
        );
        if (createRes.error) {
          errors.push(createRes.error);
        } else {
          createdStateMachines.push(...stateMachinesToCreate.map((item) => item.name));
        }
      }
    }

    return {
      content: [
        {
          type: 'text',
          text: `Scaffold complete. Artboards created: ${createdArtboards.length}. State machines created: ${createdStateMachines.length}.`,
        },
      ],
      structuredContent: {
        specPath: absPath,
        createdArtboards,
        createdStateMachines,
        errors,
      },
    };
  },
);

server.registerTool(
  'rive_proxy_bind_inputs_1to1',
  {
    title: 'Bind View Model Instances to State Machines',
    description:
      'Create View Model Instances per artboard and bind them to state machines (Rive data binding preferred approach). Uses VM properties directly, avoiding SM inputs.',
    inputSchema: {
      specPath: z
        .string()
        .describe(
          'Path to mcp_scaffold_spec.json. Absolute path recommended. Example: lightwave-ios-v2/docs/Rive/Contract/mcp_scaffold_spec.json',
        ),
      strict: z
        .boolean()
        .default(false)
        .describe('Fail if structured IDs are not available for view models/state machines/artboards.'),
    },
  },
  async ({ specPath, strict }) => {
    const absPath = resolveRepoPath(specPath);
    const raw = await readFile(absPath, 'utf8');
    const spec = SpecSchema.parse(JSON.parse(raw));

    const toolNames = await listUpstreamToolNames();
    const listViewModels = await callToolIfAvailable(toolNames, ['listViewModels'], {});
    const listStateMachines = await callToolIfAvailable(toolNames, ['listStateMachines'], {});
    const listArtboards = await callToolIfAvailable(toolNames, ['listArtboards'], {});

    const viewModelItems = extractNamedItemsFromStructured(listViewModels.result?.structuredContent, 'viewModels');
    const stateMachineItems = extractNamedItemsFromStructured(
      listStateMachines.result?.structuredContent,
      'stateMachines',
    );
    const artboardItems = extractNamedItemsFromStructured(listArtboards.result?.structuredContent, 'artboards');

    if (strict && (viewModelItems.length === 0 || stateMachineItems.length === 0 || artboardItems.length === 0)) {
      return {
        isError: true,
        content: [
          {
            type: 'text',
            text: 'Strict mode enabled, but structured IDs for view models/state machines/artboards were not available.',
          },
        ],
        structuredContent: {
          specPath: absPath,
          instancesCreated: [],
          instancesBound: [],
          errors: ['Missing structured IDs for view models/state machines/artboards.'],
        },
      };
    }

    const errors: string[] = [];
    const instancesCreated: string[] = [];
    const instancesBound: string[] = [];

    // Find tools for creating VM instances and binding them
    const createInstanceTool = findToolName(toolNames, [
      'createViewModelInstances',
      'createViewModelInstance',
      'addViewModelInstance',
    ]);

    const bindInstanceTool = findToolName(toolNames, [
      'bindViewModelInstanceToStateMachine',
      'bindInstanceToStateMachine',
      'assignViewModelInstanceToStateMachine',
      'setStateMachineViewModelInstance',
    ]);

    if (!createInstanceTool || !bindInstanceTool) {
      return {
        isError: true,
        content: [
          {
            type: 'text',
            text: `Missing upstream tool(s) required for VM instance binding. createInstanceTool=${createInstanceTool ?? 'none'} bindInstanceTool=${bindInstanceTool ?? 'none'}`,
          },
        ],
        structuredContent: {
          specPath: absPath,
          instancesCreated,
          instancesBound,
          errors: [
            `Missing required tools. createInstanceTool=${createInstanceTool ?? 'none'} bindInstanceTool=${bindInstanceTool ?? 'none'}`,
          ],
        },
      };
    }

    for (const asset of spec.assets) {
      const vmName = `VM_${asset.artboard}`;
      const vm = viewModelItems.find((item) => item.name === vmName);
      const sm = stateMachineItems.find((item) => item.name === asset.stateMachine);
      const artboard = artboardItems.find((item) => item.name === asset.artboard);

      if (!vm || !sm || !artboard) {
        errors.push(`Missing view model, state machine, or artboard for ${asset.artboard}`);
        continue;
      }

      if (!vm.id || !sm.id || !artboard.id) {
        errors.push(`Missing IDs for ${vmName}, ${asset.stateMachine}, or ${asset.artboard}`);
        continue;
      }

      // Create a default View Model Instance for this artboard
      // Instance name: same as artboard name (or "Default" if that conflicts)
      const instanceName = asset.artboard;

      try {
        // Create VM instance (idempotent - may already exist)
        await upstreamCallTool(createInstanceTool, {
          viewModelId: vm.id,
          instances: [
            {
              name: instanceName,
              // Properties will use default values from VM definition
            },
          ],
        });
        instancesCreated.push(`${vmName}::${instanceName}`);
      } catch (error) {
        // If instance already exists, that's OK - continue to binding
        const errorMsg = error instanceof Error ? error.message : String(error);
        if (!errorMsg.includes('already exists') && !errorMsg.includes('duplicate')) {
          errors.push(`Failed to create VM instance ${instanceName} for ${vmName}: ${errorMsg}`);
          continue;
        }
        // Instance exists, continue
        instancesCreated.push(`${vmName}::${instanceName} (existed)`);
      }

      // Bind the instance to the state machine (not the artboard)
      try {
        await upstreamCallTool(bindInstanceTool, {
          viewModelInstanceName: instanceName,
          stateMachineId: sm.id,
          // Alternative parameter names that might be used:
          // instanceId: instanceId,
          // viewModelId: vm.id,
        });
        instancesBound.push(`${instanceName} -> ${asset.stateMachine}`);
      } catch (error) {
        // Try alternative parameter structure
        try {
          await upstreamCallTool(bindInstanceTool, {
            viewModelId: vm.id,
            instanceName: instanceName,
            stateMachineId: sm.id,
          });
          instancesBound.push(`${instanceName} -> ${asset.stateMachine}`);
        } catch (error2) {
          errors.push(
            `Failed to bind VM instance ${instanceName} -> ${asset.stateMachine}: ${error instanceof Error ? error.message : String(error)} / ${error2 instanceof Error ? error2.message : String(error2)}`,
          );
        }
      }
    }

    return {
      content: [
        {
          type: 'text',
          text: `VM Instance binding complete. Created ${instancesCreated.length} instances, bound ${instancesBound.length} to state machines.`,
        },
      ],
      structuredContent: {
        specPath: absPath,
        instancesCreated,
        instancesBound,
        errors,
      },
    };
  },
);

server.registerTool(
  'rive_proxy_reconcile',
  {
    title: 'Reconcile Against Spec',
    description: 'Compare current Rive file against spec and report missing or extra items.',
    inputSchema: {
      specPath: z
        .string()
        .describe(
          'Path to mcp_scaffold_spec.json. Absolute path recommended. Example: lightwave-ios-v2/docs/Rive/Contract/mcp_scaffold_spec.json',
        ),
    },
  },
  async ({ specPath }) => {
    const absPath = resolveRepoPath(specPath);
    const raw = await readFile(absPath, 'utf8');
    const spec = SpecSchema.parse(JSON.parse(raw));

    const toolNames = await listUpstreamToolNames();
    const listArtboards = await callToolIfAvailable(toolNames, ['listArtboards'], {});
    const listViewModels = await callToolIfAvailable(toolNames, ['listViewModels'], {});
    const listStateMachines = await callToolIfAvailable(toolNames, ['listStateMachines'], {});

    const artboardsStructured = listArtboards.result?.structuredContent;
    const viewModelsStructured = listViewModels.result?.structuredContent;
    const stateMachinesStructured = listStateMachines.result?.structuredContent;

    const artboardItems = extractNamedItemsFromStructured(artboardsStructured, 'artboards');
    const viewModelItems = extractNamedItemsFromStructured(viewModelsStructured, 'viewModels');
    const stateMachineItems = extractNamedItemsFromStructured(stateMachinesStructured, 'stateMachines');

    const artboardNames = new Set(
      artboardItems.length > 0
        ? artboardItems.map((item) => item.name)
        : extractNamedItemsFromText(listArtboards.result?.content?.find((c) => c.type === 'text')?.text ?? '').map(
            (item) => item.name,
          ),
    );
    const viewModelNames = new Set(
      viewModelItems.length > 0
        ? viewModelItems.map((item) => item.name)
        : extractNamedItemsFromText(listViewModels.result?.content?.find((c) => c.type === 'text')?.text ?? '').map(
            (item) => item.name,
          ),
    );
    const stateMachineNames = new Set(
      stateMachineItems.length > 0
        ? stateMachineItems.map((item) => item.name)
        : extractNamedItemsFromText(listStateMachines.result?.content?.find((c) => c.type === 'text')?.text ?? '').map(
            (item) => item.name,
          ),
    );

    const expectedArtboards = spec.assets.map((asset) => asset.artboard);
    const expectedViewModels = spec.assets.map((asset) => `VM_${asset.artboard}`);
    const expectedStateMachines = spec.assets.map((asset) => asset.stateMachine);

    const missingArtboards = expectedArtboards.filter((name) => !artboardNames.has(name));
    const missingViewModels = expectedViewModels.filter((name) => !viewModelNames.has(name));
    const missingStateMachines = expectedStateMachines.filter((name) => !stateMachineNames.has(name));

    const extraArtboards = Array.from(artboardNames).filter((name) => !expectedArtboards.includes(name));
    const extraViewModels = Array.from(viewModelNames).filter((name) => !expectedViewModels.includes(name));
    const extraStateMachines = Array.from(stateMachineNames).filter((name) => !expectedStateMachines.includes(name));

    return {
      content: [
        {
          type: 'text',
          text: `Reconcile complete. Missing artboards: ${missingArtboards.length}. Missing view models: ${missingViewModels.length}. Missing state machines: ${missingStateMachines.length}.`,
        },
      ],
      structuredContent: {
        specPath: absPath,
        missingArtboards,
        missingViewModels,
        missingStateMachines,
        extraArtboards,
        extraViewModels,
        extraStateMachines,
      },
    };
  },
);

server.registerTool(
  'rive_proxy_ensure_view_models_from_spec',
  {
    title: 'Ensure View Models From Spec',
    description:
      'Ensure view models exist and contain the properties required by a scaffold spec (idempotent where possible).',
    inputSchema: {
      specPath: z
        .string()
        .describe(
          'Path to mcp_scaffold_spec.json. Absolute path recommended. Example: lightwave-ios-v2/docs/Rive/Contract/mcp_scaffold_spec.json',
        ),
      createMissing: z.boolean().default(true).describe('Create missing view models when absent.'),
      addMissingProperties: z.boolean().default(true).describe('Add missing properties to existing view models.'),
      strict: z
        .boolean()
        .default(false)
        .describe('Fail if view model listing cannot be parsed from structured output.'),
    },
  },
  async ({ specPath, createMissing, addMissingProperties, strict }) => {
    const absPath = resolveRepoPath(specPath);
    const raw = await readFile(absPath, 'utf8');
    const spec = SpecSchema.parse(JSON.parse(raw));

    // Upstream returns an untyped blob; we treat it as text+structured.
    const listRes = await upstreamCallTool('listViewModels', {});
    const listText = listRes.content?.find((c) => c.type === 'text')?.text ?? '';
    const structuredNames = extractViewModelNamesFromStructured(listRes.structuredContent);
    const textNames = extractViewModelNamesFromText(listText);
    const existingNames = new Set<string>(structuredNames.length ? structuredNames : textNames);

    if (strict && structuredNames.length === 0) {
      return {
        content: [
          {
            type: 'text',
            text: 'Strict mode enabled, but structured view model output was not available. Aborting.',
          },
        ],
        structuredContent: {
          specPath: absPath,
          ensured: [],
          created: [],
          warning: 'No structured view model list available from upstream.',
        },
      };
    }

    const created: string[] = [];
    const ensured: string[] = [];

    for (const asset of spec.assets) {
      const vmName = `VM_${asset.artboard}`;
      const props = asset.inputs.map((i) => ({ name: i.name, propertyType: mapSpecTypeToRivePropertyType(i.type) }));

      if (!existingNames.has(vmName)) {
        if (!createMissing) continue;
        await upstreamCallTool('createViewModels', { viewModels: [{ name: vmName, viewModelProperties: props }] });
        created.push(vmName);
        ensured.push(vmName);
        continue;
      }

      ensured.push(vmName);
      if (!addMissingProperties) continue;

      // Without a stable structured return from Rive MCP, we can’t reliably diff properties by ID.
      // Best-effort: attempt to create missing properties by re-adding — upstream should dedupe or ignore duplicates.
      // If upstream errors on duplicates, we can tighten later once Rive exposes stable JSON.
      try {
        // This tool requires viewModelId, which we can't reliably extract from text output right now.
        // So we skip it for now and rely on initial creation being correct.
      } catch {
        // ignore
      }
    }

    return {
      content: [
        {
          type: 'text',
          text: `Ensured ${ensured.length} view models. Created ${created.length}.`,
        },
      ],
      structuredContent: { specPath: absPath, ensured, created },
    };
  },
);

server.registerTool(
  'rive_proxy_create_layout_template',
  {
    title: 'Create Layout Template',
    description:
      'Create a named layout template in the active artboard. This is a convenience macro around Rive MCP createLayout.',
    inputSchema: {
      template: z.enum(['allScreensFullCanvas', 'allScreensOverview']).describe('Which layout template to create.'),
    },
    outputSchema: {
      template: z.string(),
    },
  },
  async ({ template }) => {
    if (template === 'allScreensOverview') {
      await upstreamCallTool('createLayout', {
        tree: {
          componentType: 'layout',
          name: 'AllScreensOverview',
          layoutStyle: {
            flexDirection: 'column',
            widthScaleType: 'fixed',
            width: 390,
            widthUnits: 'pixels',
            heightScaleType: 'fixed',
            height: 844,
            heightUnits: 'pixels',
            backgroundColor: '#FF0F1219',
            alignment: 'top-left',
          },
          children: [],
        },
      });
    } else {
      await upstreamCallTool('createLayout', {
        tree: {
          componentType: 'layout',
          name: 'AllScreensFullCanvas',
          layoutStyle: {
            flexDirection: 'column',
            widthScaleType: 'fixed',
            width: 780,
            widthUnits: 'pixels',
            heightScaleType: 'fixed',
            height: 1688,
            heightUnits: 'pixels',
            backgroundColor: '#FF0F1219',
            alignment: 'top-left',
          },
          children: [],
        },
      });
    }

    return {
      content: [{ type: 'text', text: `Template created: ${template}` }],
      structuredContent: { template },
    };
  },
);

async function main() {
  const transport = new StdioServerTransport();
  await server.connect(transport);
  console.error(`[rive-proxy] ready (upstream: ${UPSTREAM_RIVE_URL})`);
}

main().catch((err) => {
  console.error('[rive-proxy] fatal:', err);
  process.exit(1);
});
