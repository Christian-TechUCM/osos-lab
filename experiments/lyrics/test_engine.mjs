// SPDX-License-Identifier: GPL-3.0-only
import assert from 'node:assert/strict';
import {test} from 'node:test';
import {currentCue, currentCueAdjusted, validateTrack, SleepTimer} from './engine.mjs';

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

test('positive timing adjustment delays lyrics, including the first cue at zero', () => {
  const cues = [{start_ms:0, text:'Zero'}, {start_ms:1000, text:'One'}, {start_ms:2000, text:'Two'}];
  for (const position of [0, 1, 900, 1000, 1100, 1999, 2000, 6000, 0xffffffff]) {
    for (const delay of [-2147483648, -5000, -100, 0, 100, 5000, 2147483647]) {
      const adjusted = position - Math.max(-5000, Math.min(5000, delay));
      const expected = cues.filter(c => c.start_ms <= adjusted).length - 1;
      assert.equal(currentCueAdjusted(cues, position, delay), expected);
    }
  }
  assert.equal(currentCueAdjusted(cues, 1000, NaN), 1);
  assert.equal(cues[0].start_ms, 0);
});

test('sleep timer uses elapsed time, expires once, cancels and replaces cleanly', () => {
  const timer = new SleepTimer();
  assert.equal(timer.remaining(0), null);
  assert.equal(timer.consume(999999), false);
  timer.arm(15, 100);
  assert.equal(timer.remaining(200), 899900);
  // No player-position input exists: pause/seek/track changes cannot extend it.
  assert.equal(timer.consume(900099), false);
  assert.equal(timer.consume(900100), true);
  assert.equal(timer.consume(900101), false);
  assert.equal(timer.remaining(900101), null);
  timer.arm(30, 1000000);
  timer.arm(15, 1100000);
  assert.equal(timer.remaining(1100000), 900000);
  timer.arm(0, 1100010);
  assert.equal(timer.consume(9999999), false);
  for (const minutes of [-1, 1, 1.5, 121, NaN]) assert.throws(() => timer.arm(minutes, 0));
  for (const now of [-1, NaN, Infinity]) assert.throws(() => timer.arm(15, now));
});
