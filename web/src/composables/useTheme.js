import { ref } from 'vue';

const storageKey = 'librepad-theme';
function savedTheme() {
  try { return localStorage.getItem(storageKey) === 'light' ? 'light' : 'dark'; }
  catch { return 'dark'; }
}
export function useTheme() {
  const theme = ref(savedTheme());
  function apply() {
    document.documentElement.dataset.theme = theme.value;
    document.querySelector('meta[name="theme-color"]')?.setAttribute('content', theme.value === 'dark' ? '#101112' : '#f5f6f2');
  }
  function toggle() {
    theme.value = theme.value === 'dark' ? 'light' : 'dark';
    apply();
    try { localStorage.setItem(storageKey, theme.value); } catch { /* Still works when browser storage is disabled. */ }
  }
  apply();
  return { theme, toggle };
}
