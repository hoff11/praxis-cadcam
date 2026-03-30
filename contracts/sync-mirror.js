import { defaultMirrorSchemaDir, syncSchemaMirror } from './mirror-schema-sync.js';

const targetDir = process.argv[2] || defaultMirrorSchemaDir;
const files = syncSchemaMirror(targetDir, { clean: true });

console.log(`Synced ${files.length} schema file(s) to ${targetDir}`);