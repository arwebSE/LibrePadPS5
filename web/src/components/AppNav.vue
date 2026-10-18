<script setup>
import Icon from './Icon.vue';
defineProps({ tab: String, online: Boolean, loaded: Boolean, stopped: Boolean });
const emit = defineEmits(['update:tab']);
const tabs = ['controller', 'advanced'];
function navigate(event, index) {
  const next = { ArrowRight: (index + 1) % 2, ArrowLeft: (index + 1) % 2, Home: 0, End: 1 }[event.key];
  if (next === undefined) return;
  event.preventDefault(); emit('update:tab', tabs[next]);
  event.currentTarget.parentElement.querySelectorAll('[role="tab"]')[next].focus();
}
</script>
<template>
  <header class="app-nav">
    <div class="nav-inner">
      <a href="#main" class="brand" aria-label="LibrePadPS5 home"><span class="brand-mark"><Icon name="gamepad" :size="25" /></span><span>LibrePad<span class="font-normal text-stone-500">PS5</span></span></a>
      <nav class="nav-tabs" role="tablist" aria-label="Dashboard sections">
        <button v-for="(name, index) in tabs" :id="'tab-' + name" :key="name" role="tab" :aria-selected="tab === name" :aria-controls="'panel-' + name" :tabindex="tab === name ? 0 : -1" :class="{ selected: tab === name }" @click="emit('update:tab', name)" @keydown="navigate($event, index)"><Icon :name="name === 'controller' ? 'gamepad' : 'sliders'" :size="17" />{{ name === 'controller' ? 'Controller' : 'Advanced' }}</button>
      </nav>
      <span class="connection-label"><span class="status-dot" :class="{ green: online }"></span>{{ stopped ? 'Payload stopped' : online ? 'Console online' : loaded ? 'Console offline' : 'Connecting' }}</span>
    </div>
  </header>
</template>
