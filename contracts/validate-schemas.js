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

const ajv = new Ajv({ allErrors: true, allowUnionTypes: true });
addFormats(ajv);

const deploymentCompilerSchema = loadJson('./boundary/schemas/deployment-compiler.schema.json');
const validateDeploymentCompiler = ajv.compile(deploymentCompilerSchema);

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

if (failures > 0) {
  console.error(`\n${failures} validation failure(s)`);
  process.exit(1);
} else {
  console.log('\nAll schema validations passed.');
}
