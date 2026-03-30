import { existsSync, mkdtempSync, rmSync } from 'fs';
import { tmpdir } from 'os';
import { resolve } from 'path';
import {
  canonicalSchemaDir,
  defaultMirrorSchemaDir,
  diffSchemaDirectories,
  syncSchemaMirror,
} from './mirror-schema-sync.js';

let failures = 0;

function pass(message) {
  console.log(`PASS  ${message}`);
}

function fail(message) {
  console.error(`FAIL  ${message}`);
  failures++;
}

function reportDiff(prefix, diff) {
  if (diff.missing.length > 0) {
    fail(`${prefix} missing files: ${diff.missing.join(', ')}`);
  }
  if (diff.extra.length > 0) {
    fail(`${prefix} extra files: ${diff.extra.join(', ')}`);
  }
  if (diff.changed.length > 0) {
    fail(`${prefix} changed files: ${diff.changed.join(', ')}`);
  }
}

const tempRoot = mkdtempSync(resolve(tmpdir(), 'praxis-cadcam-mirror-'));
const generatedMirrorDir = resolve(tempRoot, 'boundary', 'schemas');

try {
  syncSchemaMirror(generatedMirrorDir, { clean: true });
  const generatedDiff = diffSchemaDirectories(canonicalSchemaDir, generatedMirrorDir);
  if (generatedDiff.ok) {
    pass('generated mirror matches canonical schemas');
  } else {
    reportDiff('generated mirror', generatedDiff);
  }

  if (existsSync(defaultMirrorSchemaDir)) {
    const liveMirrorDiff = diffSchemaDirectories(canonicalSchemaDir, defaultMirrorSchemaDir);
    if (liveMirrorDiff.ok) {
      pass('local praxis-cadcam-contracts mirror matches canonical schemas');
    } else {
      reportDiff('local praxis-cadcam-contracts mirror', liveMirrorDiff);
      console.error('Run `npm --prefix contracts run sync-mirror` to refresh the ignored mirror directory.');
    }
  } else {
    pass('ignored local mirror directory absent; CI verified generator output instead');
  }
} finally {
  rmSync(tempRoot, { recursive: true, force: true });
}

if (failures > 0) {
  process.exit(1);
}