import crypto from 'crypto';
import { readFileSync } from 'fs';
import { resolve, dirname } from 'path';
import { fileURLToPath } from 'url';

const __dirname = dirname(fileURLToPath(import.meta.url));

function copyIfPresent(source, target, key) {
  if (source && Object.prototype.hasOwnProperty.call(source, key)) {
    target[key] = source[key];
  }
}

function projectRefIdentity(root, key, requiredFields, optionalFields = []) {
  const out = {};
  if (!root || typeof root !== 'object' || !root[key] || typeof root[key] !== 'object') {
    return out;
  }

  const ref = root[key];
  for (const field of requiredFields) {
    copyIfPresent(ref, out, field);
  }
  for (const field of optionalFields) {
    copyIfPresent(ref, out, field);
  }
  return out;
}

function projectTargetSelection(mapping) {
  const out = {};
  if (!mapping || typeof mapping !== 'object' || !mapping.targetSelection || typeof mapping.targetSelection !== 'object') {
    return out;
  }

  const targetSelection = mapping.targetSelection;
  out.controllerBase = projectRefIdentity(targetSelection, 'controllerBase', ['id', 'version']);
  out.machineProfile = projectRefIdentity(targetSelection, 'machineProfile', ['id', 'version']);
  out.policyPack = projectRefIdentity(targetSelection, 'policyPack', ['id', 'version']);
  return out;
}

function projectCompilerRequestMapping(root) {
  const out = {};
  if (!root || typeof root !== 'object' || !root.compilerRequestMapping || typeof root.compilerRequestMapping !== 'object') {
    return out;
  }

  const mapping = root.compilerRequestMapping;
  copyIfPresent(mapping, out, 'machineCapabilityId');
  out.targetSelection = projectTargetSelection(mapping);
  return out;
}

export function deploymentCompilerSemanticProjection(deploymentCompiler) {
  const projection = {};
  copyIfPresent(deploymentCompiler, projection, 'deploymentCompilerId');
  copyIfPresent(deploymentCompiler, projection, 'deploymentCompilerVersion');
  copyIfPresent(deploymentCompiler, projection, 'schemaVersion');

  projection.machineDeploymentRef = projectRefIdentity(
    deploymentCompiler,
    'machineDeploymentRef',
    ['deploymentId', 'deploymentVersion']
  );

  projection.capabilityProfileRef = projectRefIdentity(
    deploymentCompiler,
    'capabilityProfileRef',
    ['capabilityId', 'capabilityVersion'],
    ['digest', 'checksum', 'hash']
  );

  projection.controllerBaseRef = projectRefIdentity(
    deploymentCompiler,
    'controllerBaseRef',
    ['controllerBaseId', 'controllerBaseVersion'],
    ['digest', 'checksum', 'hash']
  );

  projection.policyPackRef = projectRefIdentity(
    deploymentCompiler,
    'policyPackRef',
    ['policyPackId', 'policyPackVersion'],
    ['digest', 'checksum', 'hash']
  );

  projection.compilerPins = projectRefIdentity(
    deploymentCompiler,
    'compilerPins',
    ['compilerVersion', 'compilerBuild', 'irVersion', 'emitterVersion']
  );

  projection.compilerRequestMapping = projectCompilerRequestMapping(deploymentCompiler);

  const frozen = {};
  if (deploymentCompiler && typeof deploymentCompiler === 'object' && deploymentCompiler.frozen && typeof deploymentCompiler.frozen === 'object') {
    copyIfPresent(deploymentCompiler.frozen, frozen, 'isFrozen');
  }
  projection.frozen = frozen;

  return projection;
}

export function canonicalizeJson(value) {
  if (Array.isArray(value)) {
    return value.map(canonicalizeJson);
  }
  if (value && typeof value === 'object') {
    return Object.keys(value)
      .sort()
      .reduce((acc, key) => {
        acc[key] = canonicalizeJson(value[key]);
        return acc;
      }, {});
  }
  return value;
}

export function computeDeploymentCompilerDigest(deploymentCompiler) {
  const projection = deploymentCompilerSemanticProjection(deploymentCompiler);
  const canonical = canonicalizeJson(projection);
  const payload = JSON.stringify(canonical);
  const hex = crypto.createHash('sha256').update(payload, 'utf8').digest('hex');
  return `sha256:${hex}`;
}

function loadJson(pathArg) {
  return JSON.parse(readFileSync(resolve(__dirname, pathArg), 'utf8'));
}

if (process.argv[1] && fileURLToPath(import.meta.url) === resolve(process.argv[1])) {
  const relPath = process.argv[2];
  if (!relPath) {
    console.error('Usage: node compute-digest.js <path-to-json>');
    process.exit(1);
  }

  const data = loadJson(relPath);
  console.log(computeDeploymentCompilerDigest(data));
}
