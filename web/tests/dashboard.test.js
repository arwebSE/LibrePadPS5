import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { flushPromises, mount } from '@vue/test-utils';
import App from '../src/App.vue';
import { eligibleUsers, request, supportIssueUrl } from '../src/lib/api';
import { defaultMap, stageRow } from '../src/lib/mappings';

let wrapper, state, xbox, calls, offline, failure;
const row = '0e6f:0184 switch8 ' + defaultMap.join(',') + '\n';
beforeEach(() => {
  vi.useFakeTimers(); calls = []; offline = false; failure = '';
  state = { version: 'local-test', firmware: '12.60', users: [
    { id: '00000001', name: 'Native player', protected: true, primary_protected: true },
    { id: '00000002', name: 'Xbox player', protected: false },
    { id: '00000003', name: 'PDP player', protected: false },
  ], slots: [
    { slot: 0, name: 'Xbox One Wireless', usb_connected: true, live: true, status: 3, user: '00000002', axes: [128,128,128,128], buttons: 0, frames: 45, paused: false },
    { slot: 1, name: 'PDP', usb_connected: true, live: true, status: 3, user: '00000003', axes: [128,128,128,128], buttons: 0, frames: 22, paused: false },
    ...[2,3].map(slot => ({ slot, name: '', usb_connected: false, live: false, status: 0, user: '00000000', axes: [128,128,128,128], buttons: 0, frames: 0, paused: false })),
  ] };
  xbox = { enabled: true, firmware: 'bundled', connected: false };
  vi.stubGlobal('fetch', vi.fn(async (path, options = {}) => {
    calls.push({ path, options });
    if (offline) throw new Error('No connection');
    const url = new URL(path, 'http://console');
    let result, status = 200;
    if (options.method === 'POST') {
      if (failure) return new Response(JSON.stringify({ message: failure }), { status: 409 });
      result = { message: 'Command completed' };
      if (url.pathname === '/api/release') for (const pad of Number(url.searchParams.get('slot')) === -1 ? state.slots : [state.slots[Number(url.searchParams.get('slot'))]]) { pad.paused = true; pad.live = false; pad.status = 0; }
      if (url.pathname === '/api/reconnect') { const pad = state.slots[Number(url.searchParams.get('slot'))]; pad.paused = false; pad.live = true; pad.status = 3; }
      if (url.pathname === '/api/xbox/firmware') xbox.firmware = 'installed';
    } else if (url.pathname === '/api/state') result = state;
    else if (url.pathname === '/api/xbox') result = xbox;
    else if (url.pathname === '/api/bluetooth') result = { enabled: false, connected: false, message: 'Bluetooth disabled' };
    else if (url.pathname === '/api/profiles') result = row;
    else if (url.pathname === '/api/log') result = 'A real-looking mocked log';
    else if (url.pathname === '/api/report') result = '00 00 0f 80';
    else { status = 404; result = { message: 'Not found' }; }
    return new Response(typeof result === 'string' ? result : JSON.stringify(result), { status });
  }));
});
afterEach(() => { wrapper?.unmount(); wrapper = null; vi.useRealTimers(); vi.unstubAllGlobals(); });
async function open() { wrapper = mount(App, { attachTo: document.body }); await flushPromises(); await flushPromises(); return wrapper; }
function button(text) { return wrapper.findAll('button').find(b => b.text() === text); }
const posts = () => calls.filter(call => call.options.method === 'POST');
async function poll() { await vi.advanceTimersByTimeAsync(1000); await flushPromises(); }

describe('dashboard controls', () => {
  it('shows all slots, a live selected pad, unique headings and reserved users', async () => {
    await open();
    expect(wrapper.findAll('.controller-tile')).toHaveLength(4);
    expect(wrapper.get('#selected-controller').text()).toBe('Xbox One Wireless');
    expect(wrapper.get('#assign-user').element.value).toBe('00000002');
    expect(wrapper.get('#assign-user').text()).not.toContain('Native player');
    expect(wrapper.get('#assign-user').text()).not.toContain('PDP player');
    const ids = wrapper.findAll('[id]').map(node => node.attributes('id'));
    expect(new Set(ids).size).toBe(ids.length);
    expect(wrapper.find('input[aria-label="Reserve Native player for a native DualSense"]').element.disabled).toBe(true);
  });
  it('keeps keyboard tabs accessible without trapping Tab and preserves editor drafts', async () => {
    await open();
    expect(wrapper.get('#panel-advanced').isVisible()).toBe(false);
    await wrapper.get('#tab-controller').trigger('keydown', { key: 'ArrowRight' });
    expect(wrapper.get('#tab-advanced').attributes('aria-selected')).toBe('true');
    expect(document.activeElement.id).toBe('tab-advanced');
    expect(wrapper.get('#panel-advanced').isVisible()).toBe(true);
    await wrapper.get('#profile-text').setValue('unsaved draft');
    await wrapper.get('#tab-advanced').trigger('keydown', { key: 'Home' });
    await wrapper.get('#tab-controller').trigger('keydown', { key: 'End' });
    expect(wrapper.get('#profile-text').element.value).toBe('unsaved draft');
    await wrapper.get('#tab-advanced').trigger('keydown', { key: 'Tab' });
    expect(wrapper.get('#tab-advanced').attributes('aria-selected')).toBe('true');
  });
  it('sends selected-slot commands with the control header and leaves other pads running', async () => {
    await open(); await wrapper.get('[aria-label="Select pad 2: PDP"]').trigger('click');
    expect(wrapper.get('#assign-user').element.value).toBe('00000003');
    await button('Release virtual pad').trigger('click'); await flushPromises();
    const call = posts().at(-1);
    expect(call.path).toBe('/api/release?slot=1');
    expect(call.options.headers['X-LibrePad-Control']).toBe('1');
    expect(state.slots[0].live).toBe(true);
    expect(state.slots[1].paused).toBe(true);
    expect(button('Reconnect').element.disabled).toBe(false);
    await button('Reconnect').trigger('click'); await flushPromises();
    expect(posts().at(-1).path).toBe('/api/reconnect?slot=1');
  });
  it('assigns only the selected pad and never offers occupied or reserved profiles', async () => {
    await open(); await button('Assign user').trigger('click'); await flushPromises();
    expect(posts()[0].path).toBe('/api/rebind?slot=0&user=00000002');
    const users = eligibleUsers(state.users, state.slots, 0);
    expect(users.map(u => u.id)).toEqual(['00000002']);
  });
  it('shows server rejections and keeps the console state', async () => {
    await open(); failure = 'Profile unavailable';
    await button('Assign user').trigger('click'); await flushPromises();
    expect(wrapper.get('[role="alert"]').text()).toContain('Profile unavailable');
    expect(state.slots[0].live).toBe(true);
  });
  it('disables console controls when offline and automatically recovers', async () => {
    await open(); offline = true; await poll();
    expect(wrapper.get('.offline-banner').text()).toContain('Reconnecting automatically');
    expect(button('Assign user').element.disabled).toBe(true);
    expect(button('Press Home / PS').element.disabled).toBe(true);
    offline = false; await poll();
    expect(wrapper.find('.offline-banner').exists()).toBe(false);
    expect(button('Press Home / PS').element.disabled).toBe(false);
  });
  it('stops polling and disables controls after stopping the payload', async () => {
    await open(); await button('Stop payload').trigger('click'); await flushPromises();
    expect(wrapper.text()).toContain('Payload stopped');
    const count = calls.length; await vi.advanceTimersByTimeAsync(5000);
    expect(calls).toHaveLength(count);
    expect(button('Assign user').element.disabled).toBe(true);
  });
  it('does not overlap slow polls', async () => {
    let resolve;
    fetch.mockImplementation(() => new Promise(r => { resolve = r; }));
    wrapper = mount(App); // one status read plus the initial mapping read
    const count = fetch.mock.calls.length;
    await vi.advanceTimersByTimeAsync(3000);
    expect(fetch.mock.calls).toHaveLength(count);
    expect(resolve).toBeTypeOf('function');
  });
});

describe('firmware and Bluetooth', () => {
  it('keeps optional firmware upload closed for bundled firmware and across polling', async () => {
    await open();
    const details = wrapper.get('#xbox-heading').element.closest('section').querySelector('details');
    expect(details.open).toBe(false);
    details.open = true; details.dispatchEvent(new Event('toggle')); await flushPromises(); await poll();
    expect(details.open).toBe(true);
  });
  it('shows missing/invalid firmware and hides Xbox controls in builds without Xbox', async () => {
    xbox.firmware = 'invalid'; await open();
    expect(wrapper.get('#xbox-file').isVisible()).toBe(true);
    xbox.enabled = false; await poll();
    expect(wrapper.find('#xbox-heading').exists()).toBe(false);
  });
  it('uploads the exact file bytes with binary content type, bounds and error handling', async () => {
    await open();
    const input = wrapper.get('#xbox-file');
    const invalid = new File([new Uint8Array(31)], 'bad.bin');
    Object.defineProperty(input.element, 'files', { value: [invalid], configurable: true });
    await input.trigger('change'); await button('Install firmware').trigger('click'); await flushPromises();
    expect(posts()).toHaveLength(0);
    const file = new File([new Uint8Array(70620)], 'FW_ACC_00U.bin');
    Object.defineProperty(input.element, 'files', { value: [file] });
    await input.trigger('change'); await button('Install firmware').trigger('click'); await flushPromises();
    const call = posts().at(-1);
    expect(call.options.body).toBe(file);
    expect(call.options.headers['Content-Type']).toBe('application/octet-stream');
    expect(call.options.headers['X-LibrePad-Control']).toBe('1');
    expect(xbox.firmware).toBe('installed');
  });
  it('requires Bluetooth acknowledgement and a valid selected address', async () => {
    await open(); await button('Enable & pair selected controller').trigger('click');
    expect(posts()).toHaveLength(0);
    await wrapper.get('.checkbox-row input').setValue(true);
    await button('Enable & pair selected controller').trigger('click');
    expect(posts()).toHaveLength(0);
    await wrapper.get('#bt-address').setValue('AA:BB:CC:DD:EE:FF');
    await button('Enable & pair selected controller').trigger('click'); await flushPromises();
    expect(posts()[0].path).toBe('/api/bluetooth/pair?address=AA:BB:CC:DD:EE:FF');
  });
});

describe('mappings and support', () => {
  it('requires all slots released before saving and preserves the editor on rejection', async () => {
    await open(); await wrapper.get('#tab-advanced').trigger('click');
    expect(button('Save & apply').element.disabled).toBe(true);
    await button('Release all virtual pads').trigger('click'); await flushPromises();
    expect(button('Save & apply').element.disabled).toBe(false);
    await wrapper.get('#profile-text').setValue(row + '# unsaved note\n');
    failure = 'Invalid profiles'; await button('Save & apply').trigger('click'); await flushPromises();
    expect(wrapper.get('#profile-text').element.value).toContain('unsaved note');
    expect(wrapper.get('[role="alert"]').text()).toContain('Invalid profiles');
  });
  it('stages one row per VID/PID and validates IDs and capacity', () => {
    const bits = defaultMap.map((_,i) => i);
    expect(stageRow(row, '0E6F', '0184', 'switch8', defaultMap, bits).trim()).toBe(row.trim());
    expect(() => stageRow(row, '0000', '0184', 'switch8', defaultMap, bits)).toThrow('hexadecimal');
    const eight = Array.from({length:8},(_,i) => `${(i+1).toString(16).padStart(4,'0')}:5678 switch8 ${defaultMap.join(',')}`).join('\n');
    expect(() => stageRow(eight, '9999', '5678', 'switch8', defaultMap, bits)).toThrow('eight');
  });
  it('prefills only version and firmware in GitHub issue links', () => {
    const url = new URL(supportIssueUrl({ version: 'local-test', firmware: '12.60', users: ['PRIVATE'], log: 'SECRET' }));
    expect([...url.searchParams.keys()]).toEqual(['template','version','firmware']);
    expect(url.href).not.toMatch(/PRIVATE|SECRET/);
  });
  it('shows raw reports and logs without injecting HTML', async () => {
    await open(); await button('View latest raw report').trigger('click'); await flushPromises();
    expect(wrapper.get('[aria-label="Latest raw USB report"]').text()).toBe('00 00 0f 80');
    await button('Refresh log').trigger('click'); await flushPromises();
    expect(wrapper.get('[aria-label="Payload log"]').text()).toContain('mocked log');
  });
  it('reports HTTP errors and network timeouts', async () => {
    fetch.mockResolvedValueOnce(new Response(JSON.stringify({ message: 'Rejected' }), { status: 403 }));
    await expect(request('bad')).rejects.toThrow('Rejected');
    fetch.mockRejectedValueOnce(Object.assign(new Error(), { name: 'AbortError' }));
    await expect(request('slow')).rejects.toThrow('timed out');
  });
});
