<script setup>
import { provideI18n } from './composables/useI18n';
import { reactive, ref } from 'vue';
import { useDashboard } from './composables/useDashboard';
import AppNav from './components/AppNav.vue';
import ControllerPanel from './components/ControllerPanel.vue';
import ConnectionsPanel from './components/ConnectionsPanel.vue';
import NativeProfiles from './components/NativeProfiles.vue';
import MappingEditor from './components/MappingEditor.vue';
import SupportPanel from './components/SupportPanel.vue';
import InputMonitor from './components/InputMonitor.vue';
import Icon from './components/Icon.vue';
const { t } = provideI18n();
const d = reactive(useDashboard());
const tab = ref('controller');
</script>
<template>
  <a class="skip-link" href="#main">{{ t('Skip to dashboard') }}</a>
  <AppNav v-model:tab="tab" :online="d.online" :loaded="d.loaded" :stopped="d.stopped" />
  <main id="main" class="page-shell" tabindex="-1">
    <div v-if="d.loaded && !d.online" class="offline-banner" role="status"><Icon name="info" /><span>{{ d.stopped ? t('LibrePadPS5 has stopped. Load the payload again to reconnect.') : t('Connection lost. Reconnecting automatically; controls are paused until the console responds.') }}</span></div>
    <section id="panel-controller" v-show="tab === 'controller'" role="tabpanel" aria-labelledby="tab-controller"><ControllerPanel :dashboard="d" /><ConnectionsPanel :dashboard="d" /></section>
    <section id="panel-advanced" v-show="tab === 'advanced'" role="tabpanel" aria-labelledby="tab-advanced"><div class="mb-8"><h1>{{ t('Advanced') }}</h1><p class="subtitle">{{ t('PS5 profile reservations, button mappings, and logs.') }}</p></div><div class="advanced-top-grid"><NativeProfiles :dashboard="d" /><InputMonitor :pad="d.selectedPad" /></div><MappingEditor class="mt-6" :dashboard="d" /><SupportPanel class="mt-6" :dashboard="d" /></section>
    <footer class="app-footer"><div><a class="footer-brand" href="https://github.com/arwebSE/LibrePadPS5" target="_blank" rel="noopener noreferrer"><Icon name="gamepad" :size="18" />LibrePadPS5<Icon name="external" :size="12" /></a><p class="caption mt-2">{{ t('USB · Xbox Wireless · Bluetooth') }}</p><p class="caption mt-1">{{ t('Local dashboard · trusted LAN only') }}</p></div><div class="footer-actions"><button class="text-button" :disabled="!d.online || !!d.pending" @click="d.action('release?slot=-1')">{{ t('Release all virtual pads') }}</button><button class="btn btn-stop" :disabled="!d.online || !!d.pending" @click="d.action('stop')"><Icon name="power" :size="16" />{{ t('Stop payload') }}</button></div></footer>
  </main>
  <Transition name="toast"><div v-if="d.notice" class="toast-message" :class="{ error: d.notice.error }" :role="d.notice.error ? 'alert' : 'status'"><span class="toast-icon"><Icon :name="d.notice.error ? 'info' : 'check'" :size="18" /></span><span>{{ t(d.notice.message) }}</span><button :aria-label="t('Dismiss message')" @click="d.notice = null"><Icon name="close" :size="17" /></button></div></Transition>
</template>
