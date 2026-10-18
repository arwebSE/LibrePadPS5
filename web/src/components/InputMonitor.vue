<script setup>
import { useId } from 'vue';
defineProps({ pad: Object });
const heading = useId();
const controls = [['↑',4,'Up'],['→',5,'Right'],['↓',6,'Down'],['←',7,'Left'],['△',12,'Triangle'],['○',13,'Circle'],['×',14,'Cross'],['□',15,'Square'],['L1',10,'L1'],['R1',11,'R1'],['L2',8,'L2'],['R2',9,'R2'],['L3',1,'L3'],['R3',2,'R3'],['PS',16,'PS'],['⋮',3,'Options'],['≡',0,'Create'],['▱',20,'Touchpad']];
const position = v => Math.max(8, Math.min(92, (v ?? 128) / 255 * 100)) + '%';
</script>
<template>
  <section class="surface input-monitor" :aria-labelledby="heading">
    <div class="section-heading"><div><h2 :id="heading">Live input</h2></div><span class="badge" :class="pad?.live ? 'badge-green' : ''"><span class="status-dot" :class="{ green: pad?.live }"></span>{{ pad?.live ? 'Receiving' : 'Standby' }}</span></div>
    <div class="stick-grid">
      <div v-for="(name, index) in ['Left stick', 'Right stick']" :key="name" class="stick-box">
        <div class="stick-plane"><span class="stick-cross horizontal"></span><span class="stick-cross vertical"></span><span class="stick-range"></span><span class="stick-position" :style="{ left: position(pad?.usb_connected ? pad.axes[index * 2] : 128), top: position(pad?.usb_connected ? pad.axes[index * 2 + 1] : 128) }"></span></div>
        <div class="flex items-center justify-between gap-2"><span class="text-xs font-medium">{{ name }}</span><span class="mono text-[10px] text-muted">{{ pad?.usb_connected ? pad.axes[index*2] + ', ' + pad.axes[index*2+1] : '128, 128' }}</span></div>
      </div>
    </div>
    <div class="button-monitor" aria-label="PS5 button states"><span v-for="[label, bit, name] in controls" :key="bit" :title="name" :aria-label="name + (pad?.buttons & (1 << bit) ? ' pressed' : ' released')" :class="{ pressed: pad?.usb_connected && (pad.buttons & (1 << bit)) }">{{ label }}</span></div>
    <p class="caption mt-4">Move a stick or press a button to check the selected gamepad.</p>
  </section>
</template>
