<script setup>
import { computed, onMounted, ref } from 'vue';
import { request } from '../lib/api';
import { targets, defaultMap, fields, drivers, profileRows, stageRow } from '../lib/mappings';
import Icon from './Icon.vue';
const props = defineProps({ dashboard: Object });
const d = props.dashboard;
const text = ref(''), chosen = ref(''), vid = ref('0e6f'), pid = ref('0184');
const layout = ref('switch8'), values = ref(fields.map(f => f[1]));
const mapping = ref([...defaultMap]), bits = ref(defaultMap.map((_, i) => i));
const loading = ref(false), loadError = ref('');
const rows = computed(() => profileRows(text.value));
const custom = computed(() => layout.value.startsWith('hid:'));
const driver = computed({ get: () => custom.value ? 'custom' : layout.value,
  set: value => { layout.value = value === 'custom' ? 'hid:' + values.value.join(':') : value; } });
function edit(row) {
  const [ids, nextLayout, nextMapping] = row.split(' ');
  if (!nextMapping) return;
  [vid.value, pid.value] = ids.split(':');
  layout.value = nextLayout;
  values.value = nextLayout.startsWith('hid:') ? nextLayout.split(':').slice(1).map(Number) : fields.map(f => f[1]);
  const entries = nextMapping.split(',');
  mapping.value = defaultMap.map((target, i) => entries[i]?.split('@')[0] || target);
  bits.value = defaultMap.map((_, i) => Number(entries[i]?.split('@')[1] ?? i));
}
async function load() {
  loading.value = true;
  try { text.value = await request('profiles'); chosen.value = rows.value[0] || ''; edit(chosen.value); loadError.value = ''; }
  catch (error) { loadError.value = error.message; }
  finally { loading.value = false; }
}
function defaults() { mapping.value = [...defaultMap]; bits.value = defaultMap.map((_,i) => i); values.value = fields.map(f => f[1]); layout.value = 'switch8'; }
function stage() {
  try { text.value = stageRow(text.value, vid.value, pid.value, layout.value, mapping.value, bits.value); d.notify('Added to editor. Release all virtual pads, then Save & apply.'); }
  catch (error) { d.notify(error.message, true); }
}
async function save() {
  if (!d.allReleased) return d.notify('Release all virtual pads and wait for removal before saving.', true);
  if (text.value.length >= 2048) return d.notify('Profiles too large (maximum 2047 characters).', true);
  if (await d.action('profiles?data=' + encodeURIComponent(text.value))) await load();
}
function exportFile() {
  const url = URL.createObjectURL(new Blob([text.value], { type: 'text/plain' }));
  const a = document.createElement('a'); a.href = url; a.download = 'controllers.conf'; a.click();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}
async function importFile(event) {
  const file = event.target.files[0];
  if (!file) return;
  if (file.size >= 2048) return d.notify('Profiles too large (maximum 2047 bytes).', true);
  try { text.value = await file.text(); d.notify('Imported to editor; not saved yet.'); }
  catch (error) { d.notify(error.message, true); }
  finally { event.target.value = ''; }
}
onMounted(load);
</script>
<template>
  <section class="surface mapping-editor" aria-labelledby="mapping-heading">
    <div class="section-heading"><div><h2 id="mapping-heading">Controller profiles</h2></div><Icon name="sliders" :size="24" /></div>
    <p class="body-copy mt-3">Recognized controllers work without a custom profile. Customize USB drivers or built-in Bluetooth mappings here. Native DualSense input stays with the PS5; Xbox Wireless uses its fixed mapping.</p>
    <p v-if="loadError" class="inline-warning mt-4">Could not load saved profiles: {{ loadError }} <button class="text-button" @click="load">Try again</button></p>
    <div class="editor-top-grid mt-6"><label><span class="field-label">Saved controller</span><select v-model="chosen" class="w-full" :disabled="loading" @change="edit(chosen)"><option v-if="!rows.length" value="">No saved profiles</option><option v-for="row in rows" :key="row" :value="row">{{ row.split(' ').slice(0,2).join(' · ') }}</option></select></label><label><span class="field-label">VID (hex)</span><input v-model="vid" maxlength="4" class="w-full mono" /></label><label><span class="field-label">PID (hex)</span><input v-model="pid" maxlength="4" class="w-full mono" /></label><label><span class="field-label">Controller driver / layout</span><select v-model="driver" class="w-full"><option v-for="[value, label] in drivers" :key="value" :value="value">{{ label }}</option></select></label></div>
    <details class="subtle-details mt-5"><summary>Raw report layout<Icon name="chevron" :size="14" /></summary><div class="pt-4"><label><span class="field-label">Report layout</span><input v-model="layout" class="w-full mono" /></label><div class="report-fields mt-4"><label v-for="([label, , min, max], i) in fields" :key="label"><span class="field-label">{{ label }}</span><input v-model.number="values[i]" type="number" :min="min" :max="max" class="w-full mono" /></label></div><p class="caption mt-4">Byte offsets include the report ID. Use -1 for absent ID, hat or axes. Raw buttons use absolute bit numbers, least-significant bit first.</p><button class="btn btn-soft mt-4" @click="driver = 'custom'">Use custom layout</button></div></details>
    <div class="mapping-grid mt-6"><label v-for="(target, i) in mapping" :key="i" class="mapping-cell"><span class="field-label">{{ layout.startsWith('driver:') ? defaultMap[i] : 'Input ' + (i+1) }}<span v-if="custom" class="text-dim"> · source bit</span></span><div class="flex items-center gap-2"><input v-if="custom" v-model.number="bits[i]" type="number" min="0" max="511" class="w-18! mono" :aria-label="'Source bit for input ' + (i+1)" /><Icon v-if="custom" name="arrow" :size="14" /><select v-model="mapping[i]" class="w-full min-w-0" :aria-label="'Output for input ' + (i+1)"><option v-for="name in targets" :key="name" :value="name">{{ name }}</option></select></div></label></div>
    <div class="mt-5 flex flex-wrap gap-3"><button class="btn btn-soft" @click="defaults">Default mapping</button><button class="btn btn-dark" @click="stage">Add / update in editor<Icon name="plus" :size="16" /></button></div>
    <details class="subtle-details mt-6" open><summary>Import / export <span class="caption">Up to 8 profiles</span></summary><div class="pt-4"><label class="sr-only" for="profile-text">Profile editor</label><textarea id="profile-text" v-model="text" class="w-full mono" spellcheck="false" rows="5"></textarea><div class="mt-3 flex flex-wrap gap-3"><button class="btn btn-soft" :disabled="loading" @click="load">Reload saved</button><button class="btn btn-soft" @click="exportFile"><Icon name="download" :size="16" />Export file</button><label class="btn btn-soft cursor-pointer">Import file<input type="file" accept=".conf,.txt" class="sr-only" @change="importFile" /></label></div></div></details>
    <div class="editor-save mt-6"><p class="caption flex-1">{{ d.allReleased ? 'All virtual pads are released. Save, then reconnect each gamepad.' : 'Release all virtual pads and wait for removal before saving. Editor changes are not saved automatically.' }}</p><button class="btn btn-soft" :disabled="!d.online || !!d.pending" @click="d.action('release?slot=-1')">Release all virtual pads</button><button class="btn btn-dark" :disabled="!d.allReleased || !d.online || !!d.pending || loading" @click="save">Save &amp; apply<Icon name="check" :size="16" /></button></div>
  </section>
</template>
