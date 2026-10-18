// Development-only preview. Every /api request stays local; never controls a console.
import { defaultMap } from '../lib/mappings';
export function enablePreview() {
  const state = {
    version: 'preview.local', firmware: '12.60',
    users: [
      { id: '00000001', name: 'Player 1', protected: true, primary_protected: true, native_connected: true },
      { id: '00000002', name: 'Player 2', protected: true, native_connected: true },
      { id: '00000003', name: 'PDP player', protected: false },
      { id: '00000004', name: 'Xbox player', protected: false },
    ],
    slots: [
      { slot: 0, name: 'Xbox One Wireless', usb_connected: true, live: true, status: 3, user: '00000004', buttons: 0, axes: [132,127,129,128], frames: 28420, paused: false },
      { slot: 1, name: 'PDP Faceoff Deluxe+', usb_connected: true, live: true, status: 3, user: '00000003', buttons: 0, axes: [128,128,128,128], frames: 18532, paused: false },
      ...[2,3].map(slot => ({slot, name: '', usb_connected: false, live: false, status: 0, user: '00000000', buttons: 0, axes: [128,128,128,128], frames: 0, paused: false})),
    ],
  };
  let profiles = '0e6f:0184 switch8 ' + defaultMap.join(',') + '\n';
  const original = window.fetch;
  window.fetch = async (input, options = {}) => {
    const url = new URL(input, location.origin);
    if (!url.pathname.startsWith('/api/')) return original(input, options);
    const action = url.pathname.slice(5), slot = state.slots[Number(url.searchParams.get('slot') || 0)];
    let result, code = 200;
    if (options.method === 'POST') {
      result = { message: 'Preview: command completed locally.' };
      if (action === 'release') for (const pad of Number(url.searchParams.get('slot')) === -1 ? state.slots : [slot]) { pad.live = false; pad.status = 0; pad.paused = true; }
      else if (action === 'reconnect') { slot.live = true; slot.status = 3; slot.paused = false; }
      else if (action === 'rebind') slot.user = url.searchParams.get('user');
      else if (action === 'protect') state.users.find(user => user.id === url.searchParams.get('user')).protected = url.searchParams.get('reserved') === '1';
      else if (action === 'profiles') profiles = url.searchParams.get('data');
    } else if (action === 'state') { state.slots.filter(p => p.live).forEach(p => p.frames += 60); result = state; }
    else if (action === 'xbox') result = { enabled: true, firmware: 'bundled', connected: true };
    else if (action === 'bluetooth') result = { enabled: false, connected: false, message: 'Bluetooth is disabled. Your USB and Xbox connections are ready.' };
    else if (action === 'profiles') result = profiles;
    else if (action === 'report') result = '0e6f:0184 · 8 bytes\n00 00 0f 80 80 80 80 00';
    else if (action === 'log') result = '[Preview] Dashboard ready. All actions remain local to this preview.';
    else if (action === 'diagnostics') result = 'LibrePadPS5 preview diagnostic report. No console was contacted.';
    else { code = 404; result = { message: 'Preview endpoint unavailable' }; }
    return new Response(typeof result === 'string' ? result : JSON.stringify(result), { status: code });
  };
}
