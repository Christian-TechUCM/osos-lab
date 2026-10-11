// SPDX-License-Identifier: GPL-3.0-only
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {currentCue, validateTrack} from './engine.mjs';

test('line boundaries, pauses and backward seeks', () => {
  const cues = [{start_ms: 1000, text: 'One'}, {start_ms: 2000, text: ''}, {start_ms: 3000, text: 'Three'}];
  for (const [position, expected] of [[0,-1], [999,-1], [1000,0], [2999,1], [3000,2], [1500,0], [1500,0], [0,-1]]) {
    assert.equal(currentCue(cues, position), expected);
  }
  assert.equal(currentCue([], 5000), -1);
});
test('invalid imports are rejected before switching tracks', () => {
  const data = {schema: 1, mode: 'synced', track: {title: 'Demo', artist: 'Demo', duration_ms: 10000}, cues: [{start_ms: 0, text: 'One'}]};
  assert.equal(validateTrack(data), data);
  for (const invalid of [{}, {...data, cues: []}, {...data, schema: 2}, {...data, cues: [{start_ms: -1, text: 'Bad'}]}, {...data, cues: [{start_ms: 0, text: 'a'.repeat(2049)}]}]) {
    assert.throws(() => validateTrack(invalid));
  }
});
