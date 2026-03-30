import {
  existsSync,
  mkdirSync,
  readdirSync,
  readFileSync,
  rmSync,
  writeFileSync,
} from 'fs';
import { dirname, resolve } from 'path';
import { fileURLToPath } from 'url';

const __dirname = dirname(fileURLToPath(import.meta.url));

export const canonicalSchemaDir = resolve(__dirname, 'boundary', 'schemas');
export const defaultMirrorSchemaDir = resolve(
  __dirname,
  '..',
  'praxis-cadcam-contracts',
  'boundary',
  'schemas'
);

export function listSchemaFiles(dirPath) {
  return readdirSync(dirPath)
    .filter((name) => name.endsWith('.json'))
    .sort();
}

export function syncSchemaMirror(targetDir, { clean = true } = {}) {
  mkdirSync(targetDir, { recursive: true });

  const sourceFiles = listSchemaFiles(canonicalSchemaDir);
  if (clean && existsSync(targetDir)) {
    for (const existing of listSchemaFiles(targetDir)) {
      if (!sourceFiles.includes(existing)) {
        rmSync(resolve(targetDir, existing), { force: true });
      }
    }
  }

  for (const fileName of sourceFiles) {
    const content = readFileSync(resolve(canonicalSchemaDir, fileName), 'utf8');
    writeFileSync(resolve(targetDir, fileName), content, 'utf8');
  }

  return sourceFiles;
}

export function diffSchemaDirectories(leftDir, rightDir) {
  const leftFiles = existsSync(leftDir) ? listSchemaFiles(leftDir) : [];
  const rightFiles = existsSync(rightDir) ? listSchemaFiles(rightDir) : [];

  const missing = leftFiles.filter((fileName) => !rightFiles.includes(fileName));
  const extra = rightFiles.filter((fileName) => !leftFiles.includes(fileName));
  const changed = [];

  for (const fileName of leftFiles) {
    if (!rightFiles.includes(fileName)) {
      continue;
    }

    const leftContent = readFileSync(resolve(leftDir, fileName), 'utf8');
    const rightContent = readFileSync(resolve(rightDir, fileName), 'utf8');
    if (leftContent !== rightContent) {
      changed.push(fileName);
    }
  }

  return {
    missing,
    extra,
    changed,
    ok: missing.length === 0 && extra.length === 0 && changed.length === 0,
  };
}