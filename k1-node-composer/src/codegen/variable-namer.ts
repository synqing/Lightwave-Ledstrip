/**
 * Variable naming for C++ code generation.
 *
 * Maps node IDs and port names to clean, collision-free C++ identifiers.
 * Format: n{index}_{sanitisedPortName}
 */

/**
 * Generate a C++ variable name from a node's topo-sort index and port name.
 *
 * Examples:
 *   makeVarName('abc123', 'out', 0)        => 'n0_out'
 *   makeVarName('def456', 'normalised', 1) => 'n1_normalised'
 *   makeVarName('ghi789', 'hsv-rgb', 2)    => 'n2_hsv_rgb'
 */
export function makeVarName(
  _nodeId: string,
  portName: string,
  index: number,
): string {
  const sanitised = portName.replace(/[^a-zA-Z0-9]/g, '_');
  return `n${index}_${sanitised}`;
}

/**
 * Generate a C++ member variable name for stateful nodes.
 *
 * Examples:
 *   makeMemberName('ema-smooth', 0, 'ema') => 'm_n0_ema'
 *   makeMemberName('max-follower', 3, 'follower') => 'm_n3_follower'
 */
export function makeMemberName(
  _nodeType: string,
  index: number,
  stateName: string,
): string {
  const sanitised = stateName.replace(/[^a-zA-Z0-9]/g, '_');
  return `m_n${index}_${sanitised}`;
}
