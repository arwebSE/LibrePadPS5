<script setup>
import { computed } from 'vue';
import Icon from './Icon.vue';
const props = defineProps({ dashboard: Object });
const d = props.dashboard;
const unknown = computed(() => d.state.users.some(u => u.native_connected === null));
async function protect(event, profile) {
  const previous = profile.protected;
  const next = event.target.checked;
  event.target.checked = previous;
  await d.action(`protect?user=${profile.id}&reserved=${next ? 1 : 0}`);
}
</script>
<template>
  <section class="surface" aria-labelledby="native-heading"><div class="section-heading"><div><h2 id="native-heading">Input &amp; native controllers</h2></div><Icon name="shield" :size="24" /></div><p class="body-copy mt-3">Reserve the signed-in PS5 profiles used by your DualSense controllers. Reserved users stay unavailable to virtual gamepads.</p><div class="profile-list"><label v-for="user in d.state.users" :key="user.id" class="profile-row"><span class="avatar">{{ user.name.slice(0,1).toUpperCase() }}</span><span class="flex-1"><span class="block text-sm font-semibold">{{ user.name }}</span><span class="caption">{{ user.protected ? 'Reserved for DualSense' : 'Available for a gamepad' }}</span></span><input type="checkbox" class="switch" :checked="user.protected" :disabled="user.primary_protected || !d.online || !!d.pending" :aria-label="'Reserve ' + user.name + ' for a native DualSense'" @change="protect($event, user)" /></label><p v-if="!d.state.users.length" class="caption py-4">Sign in a PS5 user to manage profile reservations.</p></div><p class="caption">{{ unknown ? 'Native connection status is unavailable from this payload. Reserve each DualSense profile explicitly.' : 'Native DualSense controllers remain managed by the PS5.' }}</p><p class="caption mt-3">If a DualSense cannot pair, connect it by USB and press PS.</p></section>
</template>
