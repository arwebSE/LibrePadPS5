<script setup>
defineProps({ name: String, size: { default: 20 } });
const paths = {
  sun: 'M12 8a4 4 0 1 0 0 8 4 4 0 0 0 0-8ZM12 2v2m0 16v2M2 12h2m16 0h2M5 5l1.5 1.5m11 11L19 19M5 19l1.5-1.5m11-11L19 5',
  moon: 'M20.5 14A9 9 0 0 1 10 3.5 9 9 0 1 0 20.5 14Z',
  gamepad: 'M6 8h12c2 0 3 2 3.5 5l.5 4c.3 2-2 3-3.2 1.5L16 16H8l-2.8 2.5C4 20 1.7 19 2 17l.5-4C3 10 4 8 6 8Z M6 11v4m-2-2h4m8-1h.01m3 2h.01',
  sliders: 'M4 7h9m4 0h3M4 17h3m4 0h9M13 4v6M7 14v6',
  arrow: 'M5 12h14m-6-6 6 6-6 6',
  chevron: 'm9 5 7 7-7 7',
  plus: 'M12 5v14M5 12h14',
  check: 'm5 12 4 4L19 6',
  close: 'm6 6 12 12M6 18 18 6',
  usb: 'M12 3v15m-3-12 3-3 3 3M12 14l5-4V7M12 17l-5-4v-3m-2 0h4M15 5h4v2h-4M10 20a2 2 0 1 0 4 0 2 2 0 0 0-4 0',
  wireless: 'M3 8a15 15 0 0 1 18 0M6 12a10 10 0 0 1 12 0M9 16a5 5 0 0 1 6 0M12 20h.01',
  bluetooth: 'm7 7 10 10-5 4V3l5 4L7 17',
  shield: 'm12 3 8 3v6c0 5-8 9-8 9s-8-4-8-9V6l8-3Zm-4 9 3 3 5-6',
  user: 'M20 21v-2a6 6 0 0 0-6-6h-4a6 6 0 0 0-6 6v2M12 9a3 3 0 1 0 0-6 3 3 0 0 0 0 6Z',
  refresh: 'M20 8a8 8 0 1 0 0 8M20 3v5h-5',
  power: 'M12 2v10M6 5a9 9 0 1 0 12 0',
  download: 'M12 3v12m-5-5 5 5 5-5M4 17v4h16v-4',
  code: 'm8 5-6 7 6 7m8-14 6 7-6 7m-3-16-2 18',
  external: 'M14 3h7v7m0-7L10 14M10 3H3v18h18v-7',
  info: 'M12 8h.01M12 11v6M22 12a10 10 0 1 1-20 0 10 10 0 0 1 20 0Z',
};
</script>
<template><svg :width="size" :height="size" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path :d="paths[name] || paths.gamepad" /></svg></template>
