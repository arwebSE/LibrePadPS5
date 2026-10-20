<script setup>
import { useI18n } from '../composables/useI18n';
import { computed, ref, watch } from 'vue';
import { eligibleUsers } from '../lib/api';
import Icon from './Icon.vue';
import GamepadArt from './GamepadArt.vue';
import InputMonitor from './InputMonitor.vue';
const { t, n } = useI18n();
const props = defineProps({ dashboard: Object });
const d = props.dashboard;
const pad = computed(() => d.selectedPad);
const eligible = computed(() => eligibleUsers(d.state.users, d.pads, pad.value?.slot));
const user = ref('');
watch(() => [pad.value?.slot, pad.value?.user, eligible.value.map(u => u.id).join(',')], ([slot], previous) => {
  if (slot !== previous?.[0] || !eligible.value.some(u => u.id === user.value))
    user.value = eligible.value.find(u => u.id === pad.value?.user)?.id || eligible.value.find(u => u.foreground)?.id || eligible.value[0]?.id || '';
}, { immediate: true });
const assigned = computed(() => d.state.users.find(u => u.id === pad.value?.user));
const status = computed(() => !pad.value?.usb_connected ? t('Not connected') : pad.value.paused ? t('Released') : pad.value.live ? t('Active') : t('Registering'));
const locked = computed(() => !d.online || !!d.pending);
function connection(pad) { return /xbox.*wireless/i.test(pad?.name) ? ['wireless', 'Xbox Wireless'] : /bluetooth/i.test(pad?.name) ? ['bluetooth', t('Bluetooth')] : ['usb', t('USB connection')]; }
</script>
<template>
  <div class="mb-6 flex flex-wrap items-end justify-between gap-4"><div><h1>{{ t('Controllers') }}</h1><p class="subtitle">{{ t('Select a gamepad to check input or assign a PS5 user.') }}</p></div><div class="console-meta"><span class="status-dot" :class="{ green: d.online }"></span>PS5 <span class="text-dim">/</span> FW {{ d.state.firmware || '...' }}<span class="meta-divider"></span><span class="mono">{{ d.state.version || t('Connecting...') }}</span></div></div>
  <div class="mb-4 flex items-center justify-between"><h2 class="text-sm!">{{ t('Your controllers') }} <span class="count-bubble">{{ d.connected }}</span></h2><p class="caption">{{ t('{count} active', { count: n(d.active) }) }} <span class="mx-1 text-dim">/</span> {{ t('4 virtual slots') }}</p></div>
  <div class="controller-grid">
    <button v-for="item in d.pads.length ? d.pads : Array.from({length:4}, (_,slot) => ({slot}))" :key="item.slot" class="controller-tile" :class="{ 'is-selected': d.selectedSlot === item.slot, 'is-empty': !item.usb_connected }" :aria-pressed="d.selectedSlot === item.slot" :aria-label="t('Select pad {slot}: {name}', { slot: item.slot + 1, name: item.name || t('Empty slot') })" @click="d.selectedSlot = item.slot">
      <span class="tile-top"><span class="tile-icon"><Icon :name="item.usb_connected ? connection(item)[0] : 'plus'" :size="21" /></span><span class="slot-number">0{{ item.slot + 1 }}</span><span v-if="d.selectedSlot === item.slot" class="tile-selected"><Icon name="check" :size="12" /></span></span>
      <span class="tile-name">{{ item.name || t('Available slot') }}</span><span class="tile-state"><span class="status-dot" :class="{ green: item.live }"></span>{{ !item.usb_connected ? t('Connect a gamepad') : item.paused ? t('Released') : item.live ? t('Controller active') : t('Registering') }}</span>
    </button>
  </div>
  <div class="workspace-grid">
    <section class="controller-stage" aria-labelledby="selected-controller">
      <div class="stage-head"><div><p class="eyebrow">{{ pad?.usb_connected ? connection(pad)[1] : t('SELECTED GAMEPAD') }}</p><h2 id="selected-controller">{{ pad?.name || t('No controller connected') }}</h2></div><span class="badge badge-dark"><span class="status-dot" :class="{ green: pad?.live }"></span>{{ status }}</span></div>
      <GamepadArt :axes="pad?.usb_connected ? pad.axes : undefined" :buttons="pad?.usb_connected ? pad.buttons : 0" />
      <div class="stage-bottom"><p><span class="stage-small-label">{{ t('VIRTUAL PAD {slot}', { slot: (pad?.slot ?? d.selectedSlot ?? 0) + 1 }) }}</span><span>{{ pad?.usb_connected ? (pad.live ? t('Virtual pad active') : pad.paused ? t('Virtual pad released') : t('Registering virtual pad')) : t('Plug in a USB controller or pair your adapter.') }}</span></p><span class="mono frame-count">{{ t('{count} frames', { count: n(pad?.frames || 0) }) }}</span></div>
    </section>
    <section class="surface assignment-card" aria-labelledby="assignment-heading">
      <div class="card-icon"><Icon name="user" /></div><h2 id="assignment-heading" class="mt-5">{{ t('PS5 user') }}</h2>
      <p class="body-copy mt-3">{{ t('Each gamepad needs a separate signed-in PS5 user. Keep your DualSense on its reserved profile.') }}</p>
      <div class="assignment-status"><span class="status-dot" :class="{ green: assigned && pad?.live }"></span>{{ assigned ? t('Assigned to {name}', { name: assigned.name }) : pad?.live ? t('Choose a profile below or press Home / PS.') : t('Connect and register a gamepad first.') }}</div>
      <label for="assign-user" class="field-label">{{ t('Assign gamepad to console user') }}</label>
      <div class="flex flex-wrap gap-2"><select id="assign-user" v-model="user" class="min-w-0 flex-1" :disabled="!eligible.length || locked"><option v-if="!eligible.length" value="">{{ t('No available signed-in user') }}</option><option v-for="profile in eligible" :key="profile.id" :value="profile.id">{{ profile.name }}</option></select><button class="btn btn-dark" :disabled="!pad?.live || !user || locked" @click="d.padAction('rebind', 'user=' + user)">{{ t('Assign user') }}<Icon name="arrow" :size="16" /></button></div>
      <div class="quick-controls"><button class="btn btn-soft" :disabled="!pad?.live || locked" @click="d.padAction('home')"><Icon name="gamepad" :size="17" />{{ t('Press Home / PS') }}</button><button class="btn btn-soft" :disabled="!pad?.usb_connected || pad.live || pad.status === 1 || pad.status === 2 || locked" @click="d.padAction('reconnect')"><Icon name="refresh" :size="17" />{{ t('Reconnect') }}</button><button class="text-button mt-1" :disabled="!pad?.live || locked" @click="d.padAction('release')">{{ t('Release virtual pad') }}</button></div>
    </section>
    <InputMonitor :pad="pad" />
  </div>
</template>
