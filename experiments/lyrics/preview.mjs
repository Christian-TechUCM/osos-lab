// SPDX-License-Identifier: GPL-3.0-only
import {currentCue, validateTrack} from './engine.mjs';
const $ = id => document.getElementById(id);
const audio = $('audio');
const demo = {
  schema: 1, mode: 'synced',
  track: {title: 'A little closer', artist: 'Reprise demo', duration_ms: 30000},
  cues: [
    {start_ms: 1000, text: 'The first line finds its place'},
    {start_ms: 5000, text: 'And the next one moves with you'},
    {start_ms: 10000, text: 'A little closer to the music'},
    {start_ms: 15000, text: ''},
    {start_ms: 18000, text: 'Pause here, the words wait too'},
    {start_ms: 23000, text: 'Go back, and find your favorite line'},
    {start_ms: 28000, text: ''},
  ],
};
let track, nodes = [], active = -2, playing = false, position = 0, lastTick = 0;
let audioUrl = null, usingAudio = false, audioName = '', audioDuration = 0;
const time = ms => `${Math.floor(Math.max(0, ms) / 60000)}:${String(Math.floor(Math.max(0, ms) / 1000) % 60).padStart(2, '0')}`;

function pause() { playing = false; audio.pause(); refresh(); }
function clearAudio() {
  audio.pause();
  audio.removeAttribute('src'); audio.load();
  if (audioUrl) URL.revokeObjectURL(audioUrl);
  audioUrl = null; usingAudio = false; audioName = ''; audioDuration = 0;
}
function refreshBackground() {
  const screen = document.querySelector('.screen');
  screen.classList.toggle('ambient-off', !$('ambient').checked);
  $('background-status').textContent = screen.classList.contains('album-background') && $('ambient').checked ?
    'BLURRED ALBUM ART · STATIC' : ($('light').checked ? 'LIGHT BACKGROUND' : 'GITHUB DARK');
}
function load(data) {
  const validated = validateTrack(data);
  pause(); clearAudio();
  track = validated; position = 0; active = -2;
  document.querySelector('.screen').classList.remove('album-background');
  refreshBackground();
  $('title').textContent = track.track.title;
  $('artist').textContent = track.track.artist;
  $('seek').max = track.track.duration_ms;
  $('lines').replaceChildren();
  const lines = track.mode === 'synced' ? track.cues.map(c => c.text || '···') :
    [track.mode === 'static' ? track.text : 'No lyrics available for this song.'];
  nodes = lines.map(text => {
    const div = document.createElement('div');
    div.className = 'line'; div.textContent = text;
    $('lines').append(div);
    return div;
  });
  $('viewport').classList.toggle('static', track.mode !== 'synced');
  refresh();
}
function refresh() {
  if (!track) return;
  const duration = usingAudio && audioDuration ? audioDuration : track.track.duration_ms;
  $('seek').max = duration;
  $('seek').value = position;
  $('position').textContent = time(position);
  $('remaining').textContent = `−${time(duration - position)}`;
  $('play').textContent = playing ? 'Ⅱ' : '▶';
  $('play').setAttribute('aria-label', playing ? 'Pause' : 'Play');
  $('play-indicator').textContent = playing ? '▶' : 'Ⅱ';
  $('source').textContent = usingAudio ? 'AUDIO PLAYBACK CLOCK' : 'DEMO CLOCK';
  const index = currentCue(track.cues, position);
  $('line-count').textContent = track.mode === 'synced' ?
    (index < 0 ? 'Waiting for first line' : `${index + 1} / ${track.cues.length}`) :
    (track.mode === 'static' ? 'Untimed lyrics' : 'No lyrics');
  if (track.mode === 'synced' && index !== active) {
    if (nodes[active]) nodes[active].classList.remove('active');
    active = index;
    const node = nodes[Math.max(0, index)];
    if (index >= 0) node.classList.add('active');
    // Long lines start at the top; the whole cue remains readable by scrolling
    // the focused lyrics viewport. Normal cues are centered in the 155px area.
    const center = Math.max(0, ($('viewport').clientHeight - node.offsetHeight) / 2);
    $('lines').style.transform = `translateY(${center - node.offsetTop}px)`;
    $('viewport').scrollTop = 0;
    $('viewport').style.overflowY = node.offsetHeight > 155 ? 'auto' : '';
  }
}
async function toggle() {
  if (playing) return pause();
  const duration = usingAudio && audioDuration ? audioDuration : track.track.duration_ms;
  if (position >= duration) seek(0);
  if (usingAudio) {
    try { await audio.play(); } catch (error) {
      $('status').textContent = `Audio could not play: ${error.message}`; return;
    }
  }
  playing = true; lastTick = performance.now(); refresh();
}
function seek(ms) {
  const duration = usingAudio && audioDuration ? audioDuration : track.track.duration_ms;
  position = Math.max(0, Math.min(duration, ms));
  if (usingAudio && audio.readyState >= 1) audio.currentTime = position / 1000;
  refresh();
}
function setAudio(src, name) {
  pause(); position = 0; usingAudio = true; audioName = name;
  audio.src = src; audio.load(); refresh();
}
audio.addEventListener('loadedmetadata', () => {
  audioDuration = audio.duration * 1000;
  const mismatch = (track.track.audio_filename && audioName !== track.track.audio_filename) ||
    Math.abs(audioDuration - track.track.duration_ms) > 2000;
  $('status').textContent = mismatch ? 'Audio may not match these lyrics. Check the file and recording version.' :
    'Audio loaded. Press Play to follow the embedded line timings.';
  refresh();
});
audio.addEventListener('error', () => { pause(); $('status').textContent = 'This browser cannot play the audio. ALAC may need a temporary WAV copy; the original lyrics remain usable.'; });
audio.addEventListener('ended', () => { playing = false; position = audio.duration * 1000; refresh(); });
$('play').addEventListener('click', toggle);
$('back').addEventListener('click', () => seek(position - 10000));
$('forward').addEventListener('click', () => seek(position + 10000));
$('restart').addEventListener('click', () => seek(0));
$('seek').addEventListener('input', event => seek(Number(event.target.value)));
$('light').addEventListener('change', event => {
  document.querySelector('.screen').classList.toggle('light', event.target.checked);
  refreshBackground();
});
$('ambient').addEventListener('change', refreshBackground);
$('demo').addEventListener('click', () => { load(demo); $('status').textContent = 'Demo timing · no audio loaded.'; });
$('lyrics-file').addEventListener('change', async event => {
  const file = event.target.files[0]; if (!file) return;
  try {
    if (file.size > 512 * 1024) throw new Error('JSON export is too large.');
    load(JSON.parse(await file.text()));
    $('status').textContent = 'Lyrics loaded. Open the matching audio, or use the demo clock.';
  } catch (error) { $('status').textContent = error.message; }
  event.target.value = '';
});
$('audio-file').addEventListener('change', event => {
  const file = event.target.files[0]; if (!file) return;
  clearAudio(); audioUrl = URL.createObjectURL(file); setAudio(audioUrl, file.name);
  event.target.value = '';
});
document.addEventListener('keydown', event => {
  if (event.code === 'Space' && event.target === document.body) { event.preventDefault(); toggle(); }
});
function tick(now) {
  if (usingAudio) position = (audio.currentTime || 0) * 1000;
  else if (playing) {
    position = Math.min(track.track.duration_ms, position + now - lastTick);
    if (position >= track.track.duration_ms) playing = false;
  }
  lastTick = now; refresh(); requestAnimationFrame(tick);
}
load(demo); requestAnimationFrame(tick);
// The local server supplies only the explicitly selected song, never a library listing.
if (location.protocol.startsWith('http')) {
  fetch('/track.json').then(response => response.ok ? response.json() : null).then(data => {
    if (data) {
      load(data); setAudio('/audio', data.track.audio_filename);
      if (data.background?.album_art) {
        document.querySelector('.screen').classList.add('album-background');
        refreshBackground();
      }
    }
  }).catch(() => { $('status').textContent = 'Demo loaded. A local song can be opened using the file controls.'; });
}
