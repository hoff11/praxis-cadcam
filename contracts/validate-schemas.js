import { readFileSync } from 'fs';
import { resolve, dirname } from 'path';
import { fileURLToPath } from 'url';
import Ajv from 'ajv';
import addFormats from 'ajv-formats';
import { computeDeploymentCompilerDigest } from './compute-digest.js';

const __dirname = dirname(fileURLToPath(import.meta.url));

function loadJson(relPath) {
  return JSON.parse(readFileSync(resolve(__dirname, relPath), 'utf-8'));
}

function loadText(relPath) {
  return readFileSync(resolve(__dirname, '..', relPath), 'utf-8');
}

const ajv = new Ajv({ allErrors: true, allowUnionTypes: true });
addFormats(ajv);

const deploymentCompilerSchema = loadJson('./boundary/schemas/deployment-compiler.schema.json');
const validateDeploymentCompiler = ajv.compile(deploymentCompilerSchema);
const manifestSchema = loadJson('./boundary/schemas/manifest.schema.json');
const validateManifest = ajv.compile(manifestSchema);

let failures = 0;

function assertValid(label, validateFn, data) {
  const ok = validateFn(data);
  if (ok) {
    console.log(`  PASS  ${label}`);
  } else {
    console.error(`  FAIL  ${label}`);
    for (const err of validateFn.errors) {
      console.error(`        ${err.instancePath || '(root)'}: ${err.message}`);
    }
    failures++;
  }
}

function assertInvalid(label, validateFn, data) {
  const ok = validateFn(data);
  if (!ok) {
    console.log(`  PASS  ${label} (correctly rejected)`);
  } else {
    console.error(`  FAIL  ${label} (should have been rejected but was accepted)`);
    failures++;
  }
}

function assertEqual(label, actual, expected) {
  if (actual === expected) {
    console.log(`  PASS  ${label}`);
  } else {
    console.error(`  FAIL  ${label}`);
    console.error(`        expected: ${expected}`);
    console.error(`        actual:   ${actual}`);
    failures++;
  }
}

function assertContains(label, text, expected) {
  if (text.includes(expected)) {
    console.log(`  PASS  ${label}`);
  } else {
    console.error(`  FAIL  ${label}`);
    console.error(`        missing text: ${expected}`);
    failures++;
  }
}

function assertNotContains(label, text, forbidden) {
  if (!text.includes(forbidden)) {
    console.log(`  PASS  ${label}`);
  } else {
    console.error(`  FAIL  ${label}`);
    console.error(`        forbidden text present: ${forbidden}`);
    failures++;
  }
}

function digestInput(data) {
  const cloned = JSON.parse(JSON.stringify(data));
  delete cloned.digest;
  return cloned;
}

console.log('\ndeployment-compiler schema');
console.log('--------------------------');
assertValid(
  'deployment-compiler.valid.json',
  validateDeploymentCompiler,
  loadJson('./boundary/fixtures/deployment-compiler.valid.json')
);
assertInvalid(
  'deployment-compiler.missing-digest.invalid.json',
  validateDeploymentCompiler,
  loadJson('./boundary/fixtures/deployment-compiler.missing-digest.invalid.json')
);
assertInvalid(
  'deployment-compiler.machine-capability-id.invalid.json',
  validateDeploymentCompiler,
  loadJson('./boundary/fixtures/deployment-compiler.machine-capability-id.invalid.json')
);
assertInvalid(
  'deployment-compiler.missing-schema-version.invalid.json',
  validateDeploymentCompiler,
  loadJson('./boundary/fixtures/deployment-compiler.missing-schema-version.invalid.json')
);

console.log('\ndeployment-compiler digest');
console.log('--------------------------');

const validFixture = loadJson('./boundary/fixtures/deployment-compiler.valid.json');
const validFixtureComputedDigest = computeDeploymentCompilerDigest(digestInput(validFixture));
assertEqual(
  'deployment-compiler.valid.json self-consistent digest',
  validFixtureComputedDigest,
  validFixture.digest
);

const crossRepoVector = loadJson('./boundary/fixtures/cross-repo-vector.fanuc-3x-vmc.json');
const crossRepoComputedDigest = computeDeploymentCompilerDigest(crossRepoVector);
assertEqual(
  'cross-repo fanuc-3x-vmc digest vector',
  crossRepoComputedDigest,
  'sha256:4049ced787cffbeff001700d0078eccb1bcc4d08c36533b1423eff0d804d043e'
);

console.log('\nmanifest schema');
console.log('---------------');
assertValid(
  'manifest.valid.json',
  validateManifest,
  loadJson('./boundary/fixtures/manifest.valid.json')
);
assertInvalid(
  'manifest.missing-hashAlgorithm.invalid.json',
  validateManifest,
  loadJson('./boundary/fixtures/manifest.missing-hashAlgorithm.invalid.json')
);

console.log('\nownership wording');
console.log('-----------------');

const boundaryContracts = loadText('BOUNDARY_CONTRACTS.md');
const crossRepoLock = loadText('CROSS_REPO_CONTRACT_LOCK.md');
const deploymentLifecycle = loadText('DEPLOYMENT_LIFECYCLE.md');

assertContains(
  'BOUNDARY_CONTRACTS deployment-compiler owner-of-record',
  boundaryContracts,
  'Owner: praxis-cadcam / CADCAM boundary owner-of-record'
);
assertContains(
  'BOUNDARY_CONTRACTS approval-only workflow note',
  boundaryContracts,
  'approval may gate release workflow, but approval does not transfer schema ownership or artifact authorship away from CADCAM'
);
assertContains(
  'CROSS_REPO_CONTRACT_LOCK owner-of-record note',
  crossRepoLock,
  'deployment-compiler and manifest are CADCAM-owned artifacts.'
);
assertContains(
  'DEPLOYMENT_LIFECYCLE deployment-compiler ownership note',
  deploymentLifecycle,
  'CADCAM is the owner-of-record for this artifact and its publication flow; any cross-pillar approval is a workflow gate only.'
);
assertNotContains(
  'BOUNDARY_CONTRACTS no platform-boundary owner label remains',
  boundaryContracts,
  'Owner: platform boundary'
);

if (failures > 0) {
  console.error(`\n${failures} validation failure(s)`);
  process.exit(1);
} else {
  console.log('\nAll schema validations passed.');
}
