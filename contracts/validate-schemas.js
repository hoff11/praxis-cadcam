import { readFileSync } from 'fs';
import { resolve, dirname } from 'path';
import { fileURLToPath } from 'url';
import Ajv from 'ajv';
import addFormats from 'ajv-formats';

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

if (failures > 0) {
  console.error(`\n${failures} validation failure(s)`);
  process.exit(1);
} else {
  console.log('\nAll schema validations passed.');
}
