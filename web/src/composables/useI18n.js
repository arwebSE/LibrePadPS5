import { computed, inject, provide, ref } from 'vue';
import { messages } from '../locales/messages';

export const languages = [
  { code: 'en', name: 'English' },
  { code: 'ja', name: '日本語' },
  { code: 'es', name: 'Español' },
  { code: 'de', name: 'Deutsch' },
  { code: 'fr', name: 'Français' },
  { code: 'sv', name: 'Svenska' },
  { code: 'zh-Hans', name: '简体中文' },
];
const key = Symbol('i18n');
const storageKey = 'librepad-language';

export function matchLanguage(language) {
  const tag = String(language).toLowerCase().replace(/_/g, '-');
  // Traditional Chinese should fall back to English rather than silently use Simplified.
  if (/^zh(?:-|$)/.test(tag)) {
    if (tag.includes('-hans')) return 'zh-Hans';
    if (tag.includes('-hant') || /^zh-(tw|hk|mo)(?:-|$)/.test(tag)) return null;
    return 'zh-Hans';
  }
  return languages.find(item => item.code === tag.split('-')[0])?.code ?? null;
}

export function initialLanguage() {
  try {
    const saved = localStorage.getItem(storageKey);
    if (languages.some(item => item.code === saved)) return saved;
  } catch { /* Language detection still works when storage is blocked. */ }
  for (const language of navigator.languages?.length ? navigator.languages : [navigator.language]) {
    const match = matchLanguage(language);
    if (match) return match;
  }
  return 'en';
}

export function translate(language, message, params = {}) {
  const catalog = messages[language];
  const template = catalog && Object.prototype.hasOwnProperty.call(catalog, message) ? catalog[message] : message;
  return String(template ?? '').replace(/\{(\w+)\}/g, (token, name) =>
    Object.prototype.hasOwnProperty.call(params, name) ? String(params[name]) : token);
}

export function provideI18n() {
  const current = ref(initialLanguage());
  const locale = computed({
    get: () => current.value,
    set: value => {
      if (!languages.some(item => item.code === value)) return;
      current.value = value;
      document.documentElement.lang = value;
      try { localStorage.setItem(storageKey, value); } catch { /* Keep the session choice. */ }
    },
  });
  document.documentElement.lang = current.value;
  function t(message, params) {
    const failed = /^Request failed \((\d+)\)$/.exec(message);
    return failed ? translate(current.value, 'Request failed ({status})', { status: failed[1] })
      : translate(current.value, message, params);
  }
  const n = value => new Intl.NumberFormat(current.value).format(value);
  const i18n = { locale, languages, t, n };
  provide(key, i18n);
  return i18n;
}

export function useI18n() { return inject(key); }
