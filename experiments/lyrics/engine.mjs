// SPDX-License-Identifier: GPL-3.0-only
export function currentCue(cues, positionMs) {
  let low = 0, high = cues.length;
  while (low < high) {
    const mid = low + Math.floor((high - low) / 2);
    if (cues[mid].start_ms <= positionMs) low = mid + 1;
    else high = mid;
  }
  return low - 1;
}

export function validateTrack(data) {
  if (!data || data.schema !== 1 || !data.track ||
      typeof data.track.title !== 'string' || typeof data.track.artist !== 'string' ||
      !Number.isFinite(data.track.duration_ms) || data.track.duration_ms <= 0 ||
      data.track.duration_ms > 86400000 ||
      !['synced', 'static', 'missing'].includes(data.mode)) {
    throw new Error('This is not a supported lyrics export.');
  }
  if (!Array.isArray(data.cues) || data.cues.length > 1024 ||
      (data.mode === 'synced' && !data.cues.length)) {
    throw new Error('Invalid lyric cue count.');
  }
  let previous = -1;
  let bytes = 0;
  for (const cue of data.cues) {
    if (!Number.isInteger(cue.start_ms) || cue.start_ms <= previous ||
        cue.start_ms > 86400000 || typeof cue.text !== 'string') {
      throw new Error('Invalid lyric cue.');
    }
    const length = new TextEncoder().encode(cue.text).length;
    if (length > 2048) throw new Error('Lyric line is too long.');
    bytes += length + 12;
    previous = cue.start_ms;
  }
  if (bytes + 16 > 131072 || (data.mode === 'static' && typeof data.text !== 'string')) {
    throw new Error('Lyric text exceeds the prototype limits.');
  }
  return data;
}
