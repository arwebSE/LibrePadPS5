<script setup>
import { computed, ref, watch } from 'vue';
import { eligibleUsers } from '../lib/api';
import Icon from './Icon.vue';
import GamepadArt from './GamepadArt.vue';
import InputMonitor from './InputMonitor.vue';
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
const status = computed(() => !pad.value?.usb_connected ? 'Not connected' : pad.value.paused ? 'Released' : pad.value.live ? 'Active' : 'Registering');
const locked = computed(() => !d.online || !!d.pending);
function connection(pad) { return /xbox.*wireless/i.test(pad?.name) ? ['wireless', 'Xbox Wireless'] : /bluetooth/i.test(pad?.name) ? ['bluetooth', 'Bluetooth'] : ['usb', 'USB connection']; }
</script>
<template>
  <div class="mb-6 flex flex-wrap items-end justify-between gap-4"><div><h1>Controllers</h1><p class="subtitle">Select a gamepad to check input or assign a PS5 user.</p></div><div class="console-meta"><span class="status-dot" :class="{ green: d.online }"></span>PS5 <span class="text-dim">/</span> FW {{ d.state.firmware || '...' }}<span class="meta-divider"></span><span class="mono">{{ d.state.version || 'Connecting...' }}</span></div></div>
  <div class="mb-4 flex items-center justify-between"><h2 class="text-sm!">Your controllers <span class="count-bubble">{{ d.connected }}</span></h2><p class="caption">{{ d.active }} active <span class="mx-1 text-dim">/</span> 4 virtual slots</p></div>
  <div class="controller-grid">
    <button v-for="item in d.pads.length ? d.pads : Array.from({length:4}, (_,slot) => ({slot}))" :key="item.slot" class="controller-tile" :class="{ 'is-selected': d.selectedSlot === item.slot, 'is-empty': !item.usb_connected }" :aria-pressed="d.selectedSlot === item.slot" :aria-label="'Select pad ' + (item.slot + 1) + ': ' + (item.name || 'Empty slot')" @click="d.selectedSlot = item.slot">
      <span class="tile-top"><span class="tile-icon"><Icon :name="item.usb_connected ? connection(item)[0] : 'plus'" :size="21" /></span><span class="slot-number">0{{ item.slot + 1 }}</span><span v-if="d.selectedSlot === item.slot" class="tile-selected"><Icon name="check" :size="12" /></span></span>
      <span class="tile-name">{{ item.name || 'Available slot' }}</span><span class="tile-state"><span class="status-dot" :class="{ green: item.live }"></span>{{ !item.usb_connected ? 'Connect a gamepad' : item.paused ? 'Released' : item.live ? 'Controller active' : 'Registering' }}</span>
    </button>
  </div>
  <div class="workspace-grid">
    <section class="controller-stage" aria-labelledby="selected-controller">
      <div class="stage-head"><div><p class="eyebrow">{{ pad?.usb_connected ? connection(pad)[1] : 'SELECTED GAMEPAD' }}</p><h2 id="selected-controller">{{ pad?.name || 'No controller connected' }}</h2></div><span class="badge badge-dark"><span class="status-dot" :class="{ green: pad?.live }"></span>{{ status }}</span></div>
      <GamepadArt :axes="pad?.usb_connected ? pad.axes : undefined" :buttons="pad?.usb_connected ? pad.buttons : 0" />
      <div class="stage-bottom"><p><span class="stage-small-label">VIRTUAL PAD {{ (pad?.slot ?? d.selectedSlot ?? 0) + 1 }}</span><span>{{ pad?.usb_connected ? (pad.live ? 'Virtual pad active' : pad.paused ? 'Virtual pad released' : 'Registering virtual pad') : 'Plug in a USB controller or pair your adapter.' }}</span></p><span class="mono frame-count">{{ (pad?.frames || 0).toLocaleString() }} frames</span></div>
    </section>
    <section class="surface assignment-card" aria-labelledby="assignment-heading">
      <div class="card-icon"><Icon name="user" /></div><h2 id="assignment-heading" class="mt-5">PS5 user</h2>
      <p class="body-copy mt-3">Each gamepad needs a separate signed-in PS5 user. Keep your DualSense on its reserved profile.</p>
      <div class="assignment-status"><span class="status-dot" :class="{ green: assigned && pad?.live }"></span>{{ assigned ? 'Assigned to ' + assigned.name : pad?.live ? 'Choose a profile below or press Home / PS.' : 'Connect and register a gamepad first.' }}</div>
      <label for="assign-user" class="field-label">Assign gamepad to console user</label>
      <div class="flex flex-wrap gap-2"><select id="assign-user" v-model="user" class="min-w-0 flex-1" :disabled="!eligible.length || locked"><option v-if="!eligible.length" value="">No available signed-in user</option><option v-for="profile in eligible" :key="profile.id" :value="profile.id">{{ profile.name }}</option></select><button class="btn btn-dark" :disabled="!pad?.live || !user || locked" @click="d.padAction('rebind', 'user=' + user)">Assign user<Icon name="arrow" :size="16" /></button></div>
      <div class="quick-controls"><button class="btn btn-soft" :disabled="!pad?.live || locked" @click="d.padAction('home')"><Icon name="gamepad" :size="17" />Press Home / PS</button><button class="btn btn-soft" :disabled="!pad?.usb_connected || pad.live || pad.status === 1 || pad.status === 2 || locked" @click="d.padAction('reconnect')"><Icon name="refresh" :size="17" />Reconnect</button><button class="text-button mt-1" :disabled="!pad?.live || locked" @click="d.padAction('release')">Release virtual pad</button></div>
    </section>
    <InputMonitor :pad="pad" />
  </div>
</template>
