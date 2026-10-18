<script setup>
defineProps({ axes: { default: () => [128,128,128,128] }, buttons: { default: 0 } });
const offset = v => Math.max(-9, Math.min(9, ((v ?? 128) - 128) / 14));
</script>
<template>
  <svg class="gamepad-art" viewBox="0 0 460 260" role="img" aria-label="Controller with live stick and button input">
    <ellipse cx="230" cy="226" rx="136" ry="10" fill="#000" opacity=".12" />
    <path d="M121 56c-25 0-40 17-47 43L51 183c-8 33 12 51 36 32l43-35h200l43 35c24 19 44 1 36-32l-23-84c-7-26-22-43-47-43Z" fill="#303238" stroke="#595d65" stroke-width="2" />
    <path d="m127 55 14-15h45l8 16m72 0 8-16h45l14 15" fill="#45484f" stroke="#70757f" stroke-width="2" />
    <rect x="179" y="65" width="102" height="58" rx="14" fill="#3d4148" stroke="#575d67" />
    <rect x="196" y="54" width="68" height="3" rx="2" fill="#c6f279" />
    <path d="M109 86h14v15h15v14h-15v15h-14v-15H94v-14h15Z" fill="#16181c" stroke="#545a64" />
    <circle cx="175" cy="162" r="31" fill="#1c1f24" stroke="#545a64" /><circle :cx="175 + offset(axes[0])" :cy="162 + offset(axes[1])" r="21" fill="#3e444e" stroke="#747d8b" />
    <circle cx="285" cy="162" r="31" fill="#1c1f24" stroke="#545a64" /><circle :cx="285 + offset(axes[2])" :cy="162 + offset(axes[3])" r="21" fill="#3e444e" stroke="#747d8b" />
    <g :fill="buttons & (1 << 12) ? '#c6f279' : '#1c1f24'"><circle cx="345" cy="84" r="13" /><path d="m345 77 6 10h-12Z" fill="none" stroke="#bdc5d0" stroke-width="1.6" /></g>
    <g :fill="buttons & (1 << 13) ? '#c6f279' : '#1c1f24'"><circle cx="369" cy="108" r="13" /><circle cx="369" cy="108" r="6" fill="none" stroke="#bdc5d0" stroke-width="1.6" /></g>
    <g :fill="buttons & (1 << 14) ? '#c6f279' : '#1c1f24'"><circle cx="345" cy="132" r="13" /><path d="m340 127 10 10m-10 0 10-10" stroke="#bdc5d0" stroke-width="1.6" /></g>
    <g :fill="buttons & (1 << 15) ? '#c6f279' : '#1c1f24'"><circle cx="321" cy="108" r="13" /><rect x="316" y="103" width="10" height="10" rx="1" fill="none" stroke="#bdc5d0" stroke-width="1.6" /></g>
    <circle cx="230" cy="153" r="9" :fill="buttons & (1 << 16) ? '#c6f279' : '#626a76'" /><path d="M225 153h10m-5-5v10" stroke="#272b32" />
  </svg>
</template>
