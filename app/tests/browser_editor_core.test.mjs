import { test } from 'node:test';
import assert from 'node:assert/strict';
import { pathToFileURL } from 'node:url';

const modulePath = process.env.CATCHIM_BROWSER_CORE || 'C:/catchim-wasm-mt/bin/catchim_browser_core.mjs';
const { default: factory } = await import(pathToFileURL(modulePath).href);
const core = await factory();
function request(value) {
  const bytes = new TextEncoder().encode(JSON.stringify(value));
  const pointer = core._malloc(bytes.length + 1);
  try {
    core.HEAPU8.set(bytes, pointer); core.HEAPU8[pointer + bytes.length] = 0;
    return JSON.parse(core.UTF8ToString(core._catchim_browser_request(pointer)));
  } finally { core._free(pointer); }
}
const project = name => ({ metadata: { id: 'project-1', name, thumbnail: 'x'.repeat(250000), createdAt: '', updatedAt: '' }, settings: {}, scenes: [] });
test('C++ history accepts large projects, preserves extra browser fields, undo/redo and branching', () => {
  const first = { project: project('Before'), selection: { selectedElements: [] }, extra: { keep: 'original browser data' } };
  assert.equal(request({ op: 'load', document: first }).ok, true);
  const second = structuredClone(first); second.project.metadata.name = 'After';
  assert.equal(request({ op: 'commit', document: second }).canUndo, true);
  assert.deepEqual(request({ op: 'undo' }).document, first);
  assert.deepEqual(request({ op: 'redo' }).document, second);
  request({ op: 'undo' });
  const branch = structuredClone(first); branch.project.metadata.name = 'Branch';
  assert.equal(request({ op: 'commit', document: branch }).canRedo, false);
});
test('invalid requests do not overwrite the current project', () => {
  const previous = request({ op: 'state' }).document;
  assert.equal(request({ op: 'commit', document: { project: 42 } }).ok, false);
  assert.deepEqual(request({ op: 'state' }).document, previous);
});
