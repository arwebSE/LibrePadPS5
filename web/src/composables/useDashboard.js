import { computed, onMounted, onUnmounted, ref } from 'vue';
import { request } from '../lib/api';

export function useDashboard() {
  const state = ref({ users: [], slots: [] });
  const xbox = ref({ enabled: false });
  const bluetooth = ref({ enabled: false, message: 'Checking connection...' });
  const online = ref(false), loaded = ref(false), stopped = ref(false);
  const selectedSlot = ref(null), pending = ref(''), notice = ref(null);
  let timer, disposed = false, refreshing = false;
  const pads = computed(() => state.value.slots || []);
  const selectedPad = computed(() => pads.value.find(pad => pad.slot === selectedSlot.value));
  const connected = computed(() => pads.value.filter(pad => pad.usb_connected).length);
  const active = computed(() => pads.value.filter(pad => pad.live).length);
  const allReleased = computed(() => pads.value.length > 0 && pads.value.every(pad => pad.status === 0 && pad.paused));

  function notify(message, error = false) { notice.value = { message, error }; }
  async function refresh() {
    if (disposed || stopped.value || refreshing) return;
    refreshing = true;
    try {
      const next = await request('state');
      if (!next || !Array.isArray(next.slots) || !Array.isArray(next.users))
        throw new Error('Invalid console status response');
      if (disposed || stopped.value) return;
      state.value = next;
      online.value = true;
      loaded.value = true;
      if (selectedSlot.value === null)
        selectedSlot.value = next.slots?.find(pad => pad.usb_connected)?.slot ?? 0;
      const results = await Promise.allSettled([request('xbox'), request('bluetooth')]);
      if (disposed || stopped.value) return;
      if (results[0].status === 'fulfilled') xbox.value = results[0].value;
      else xbox.value = { ...xbox.value, unavailable: true };
      if (results[1].status === 'fulfilled') bluetooth.value = results[1].value;
      else bluetooth.value = { ...bluetooth.value, message: 'Bluetooth status unavailable' };
    } catch { if (!disposed) { online.value = false; loaded.value = true; } }
    finally { refreshing = false; }
  }
  async function action(path) {
    if (pending.value || !online.value || stopped.value) return false;
    pending.value = path;
    try {
      const result = await request(path, { method: 'POST' });
      notify(result.message);
      if (path === 'stop') { stopped.value = true; online.value = false; clearTimeout(timer); }
      else await refresh();
      return true;
    } catch (error) { notify(error.message, true); return false; }
    finally { pending.value = ''; }
  }
  function padAction(path, query = '') {
    if (!selectedPad.value) return Promise.resolve(false);
    return action(`${path}?slot=${selectedPad.value.slot}${query ? '&' + query : ''}`);
  }
  async function uploadFirmware(file) {
    if (!file || file.size < 32 || file.size > 1024 * 1024) {
      notify('Choose a firmware .bin file between 32 bytes and 1 MiB.', true); return false;
    }
    if (!online.value || pending.value || stopped.value) return false;
    pending.value = 'xbox/firmware';
    try {
      const result = await request('xbox/firmware', {
        method: 'POST', headers: { 'Content-Type': 'application/octet-stream' }, body: file,
      });
      notify(result.message); await refresh(); return true;
    } catch (error) { notify(error.message, true); return false; }
    finally { pending.value = ''; }
  }
  onMounted(async () => {
    async function poll() {
      await refresh();
      if (!disposed && !stopped.value) timer = setTimeout(poll, 1000);
    }
    poll();
  });
  onUnmounted(() => { disposed = true; clearTimeout(timer); });
  return { state, xbox, bluetooth, online, loaded, stopped, selectedSlot, selectedPad,
    pending, notice, pads, connected, active, allReleased, refresh, action, padAction,
    uploadFirmware, notify };
}
