<script setup>
import { useI18n } from '../composables/useI18n';
import Icon from './Icon.vue';
import { useTheme } from '../composables/useTheme';
const { t, locale, languages } = useI18n();
const { theme, toggle } = useTheme();
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
      <a href="#main" class="brand" :aria-label="t('LibrePadPS5 home')"><span class="brand-mark"><Icon name="gamepad" :size="25" /></span><span>LibrePad<span class="font-normal text-muted">PS5</span></span></a>
      <nav class="nav-tabs" role="tablist" :aria-label="t('Dashboard sections')">
        <button v-for="(name, index) in tabs" :id="'tab-' + name" :key="name" role="tab" :aria-selected="tab === name" :aria-controls="'panel-' + name" :tabindex="tab === name ? 0 : -1" :class="{ selected: tab === name }" @click="emit('update:tab', name)" @keydown="navigate($event, index)"><Icon :name="name === 'controller' ? 'gamepad' : 'sliders'" :size="17" />{{ name === 'controller' ? t('Controller') : t('Advanced') }}</button>
      </nav>
      <div class="nav-actions"><label class="language-picker"><span class="sr-only">{{ t('Language') }}</span><select id="ui-language" v-model="locale" :aria-label="t('Language')"><option v-for="language in languages" :key="language.code" :value="language.code" :lang="language.code">{{ language.name }}</option></select></label><span class="connection-label"><span class="status-dot" :class="{ green: online }"></span>{{ stopped ? t('Payload stopped') : online ? t('Console online') : loaded ? t('Console offline') : t('Connecting') }}</span><button class="theme-toggle" :aria-label="t('Dark mode')" :aria-pressed="theme === 'dark'" :title="theme === 'dark' ? t('Switch to light mode') : t('Switch to dark mode')" @click="toggle"><Icon :name="theme === 'dark' ? 'sun' : 'moon'" :size="19" /></button></div>
    </div>
  </header>
</template>
