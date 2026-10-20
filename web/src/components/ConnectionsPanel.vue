<script setup>
import { useI18n } from '../composables/useI18n';
import { computed, ref } from 'vue';
import Icon from './Icon.vue';
const { t } = useI18n();
const props = defineProps({ dashboard: Object });
const d = props.dashboard;
const address = ref(''), risk = ref(false), file = ref(null), opened = ref(false), input = ref(null);
const firmwareReady = computed(() => ['bundled', 'installed'].includes(d.xbox.firmware));
const locked = computed(() => !d.online || !!d.pending);
const xboxMessage = computed(() => d.xbox.unavailable ? t('Adapter status unavailable.') : d.xbox.connected ? t('Your Xbox controller is connected.') : d.xbox.firmware === 'bundled' ? t('Firmware is included. Plug in your adapter and pair.') : d.xbox.firmware === 'installed' ? t('Your installed firmware is ready. Pair your controller.') : d.xbox.firmware === 'missing' ? t('This build needs a one-time firmware install.') : t('Firmware is {status}. Install a valid image.', { status: t(d.xbox.firmware) }));
async function install() { if (await d.uploadFirmware(file.value)) { opened.value = false; file.value = null; if (input.value) input.value.value = ''; } }
function pair() {
  if (!risk.value) return d.notify('Read and acknowledge the Bluetooth warning first.', true);
  if (!/^(?:[0-9a-f]{2}:){5}[0-9a-f]{2}$/i.test(address.value.trim()))
    return d.notify('Use an address like AA:BB:CC:DD:EE:FF.', true);
  // The payload expects a validated colon-separated address, not URL escapes.
  d.action('bluetooth/pair?address=' + address.value.trim());
}
</script>
<template>
  <div class="connections-grid">
    <section v-if="d.xbox.enabled" class="surface connection-card" aria-labelledby="xbox-heading">
      <div class="flex items-start justify-between gap-4"><span class="card-icon"><Icon name="wireless" /></span><span class="badge" :class="d.xbox.connected ? 'badge-green' : ''">{{ d.xbox.connected ? t('Connected') : firmwareReady ? t('Ready to pair') : t('Setup needed') }}</span></div>
      <h2 id="xbox-heading" class="mt-5">Xbox Wireless</h2><p class="body-copy mt-2">{{ xboxMessage }}</p><p class="caption mt-4">{{ t('Pair with the buttons on the adapter and controller. Built-in Bluetooth stays disabled.') }}</p>
      <details class="subtle-details mt-5" :open="opened || !firmwareReady" @toggle="opened = $event.target.open"><summary>{{ t('Advanced firmware override') }}<Icon name="chevron" :size="14" /></summary><div class="pt-4"><p class="caption mb-4">{{ t('Optional: replace the included firmware with a local .bin file.') }} <a href="https://github.com/arwebSE/LibrePadPS5/blob/main/docs/XBOX.md#advanced" target="_blank" rel="noopener noreferrer">{{ t('Manual setup and verification') }}</a></p><label class="field-label" for="xbox-file">{{ t('Adapter firmware') }}</label><input id="xbox-file" ref="input" type="file" accept=".bin" :disabled="locked" @change="file = $event.target.files[0]" /><button class="btn btn-dark mt-3" :disabled="!file || locked" @click="install">{{ d.pending === 'xbox/firmware' ? t('Installing...') : t('Install firmware') }}</button><p class="caption mt-3">{{ t('Saved on the console. Unplug and replug the adapter after installing.') }}</p></div></details>
    </section>
    <section class="surface connection-card" aria-labelledby="bluetooth-heading">
      <div class="flex items-start justify-between gap-4"><span class="card-icon"><Icon name="bluetooth" /></span><span class="badge" :class="d.bluetooth.connected ? 'badge-green' : ''">{{ d.bluetooth.enabled ? d.bluetooth.connected ? t('Connected') : t('Enabled') : t('Off by default') }}</span></div>
      <h2 id="bluetooth-heading" class="mt-5">{{ t('Bluetooth') }}</h2><p class="body-copy mt-2">{{ t(d.bluetooth.message) }}</p><p v-if="d.bluetooth.pairing" class="caption mt-2">{{ t('{count} seconds remaining in the pairing window.', { count: d.bluetooth.pairing }) }}</p><p v-if="d.bluetooth.connected && d.bluetooth.vid" class="caption mono mt-2">USB ID {{ d.bluetooth.vid.toString(16).padStart(4,'0') }}:{{ d.bluetooth.pid.toString(16).padStart(4,'0') }}</p>
      <details class="subtle-details mt-5"><summary>{{ t('Pair a Bluetooth controller') }}<Icon name="chevron" :size="14" /></summary><div class="pt-4"><p class="caption">{{ t('Unplug USB gamepads and the Xbox adapter, then release all virtual pads. Uses shared console services; native DualSense isolation is not guaranteed. Keep a USB data cable available and stop if native input fails.') }}</p><label class="checkbox-row mt-4"><input v-model="risk" type="checkbox" />{{ t('I understand this uses shared Bluetooth services.') }}</label><label class="field-label" for="bt-address">{{ t('Controller Bluetooth address') }}</label><input id="bt-address" v-model="address" maxlength="17" placeholder="AA:BB:CC:DD:EE:FF" class="w-full mono" /><p class="caption mt-2">{{ t('Use the address from your PC\'s Bluetooth device details. Never select your native DualSense.') }}</p><button class="btn btn-dark mt-4" :disabled="locked" @click="pair">{{ t('Enable & pair selected controller') }}</button><p class="caption mt-3">{{ t('Put the controller in pairing mode. Pairing lasts 120 seconds; enable again after a payload restart.') }}</p></div></details>
      <div v-if="d.bluetooth.enabled" class="mt-4 flex flex-wrap gap-3"><button class="text-button" :disabled="locked" @click="d.action('bluetooth/disable')">{{ t('Disable Bluetooth') }}</button><button class="text-button" :disabled="locked" @click="d.action('bluetooth/forget')">{{ t('Forget selected controller') }}</button></div>
    </section>
    <aside class="native-tip"><span class="tip-icon"><Icon name="shield" :size="23" /></span><h2>{{ t('Native DualSense') }}</h2><p class="body-copy mt-3">{{ t('DualSense controllers stay with the PS5. Reserve their profiles in Advanced to keep virtual gamepads off those users.') }}</p><a class="text-button mt-6" href="https://github.com/arwebSE/LibrePadPS5/blob/main/docs/SETUP.md#multiple-controllers" target="_blank" rel="noopener noreferrer">{{ t('Multiplayer setup') }}<Icon name="external" :size="14" /></a></aside>
  </div>
</template>
