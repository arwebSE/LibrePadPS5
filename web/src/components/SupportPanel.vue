<script setup>
import { ref, computed } from 'vue';
import { request, supportIssueUrl } from '../lib/api';
import Icon from './Icon.vue';
const props = defineProps({ dashboard: Object });
const d = props.dashboard;
const log = ref(''), raw = ref(''), loading = ref(false);
const issue = computed(() => supportIssueUrl(d.state));
async function read(path) {
  loading.value = true;
  try { const result = await request(path); if (path === 'log') log.value = result; else raw.value = result; }
  catch (error) { d.notify(error.message, true); }
  finally { loading.value = false; }
}
</script>
<template>
  <section class="surface" aria-labelledby="support-heading"><div class="section-heading"><div><h2 id="support-heading">Logs &amp; support</h2></div><Icon name="code" :size="24" /></div><p class="body-copy mt-3">Download a report while the payload is running. Attach it to your GitHub issue if you need help.</p><div class="mt-5 flex flex-wrap gap-3"><a class="btn btn-dark" href="/api/diagnostics" download :aria-disabled="!d.online" @click="!d.online && $event.preventDefault()"><Icon name="download" :size="17" />Download diagnostic report (.txt)</a><a class="btn btn-soft" :href="issue" target="_blank" rel="noopener noreferrer">Open GitHub issue<Icon name="external" :size="15" /></a></div><p class="caption mt-4">Review before sharing. Reports can contain profile names/IDs and device addresses. Nothing is uploaded automatically.</p><div class="mt-6 flex flex-wrap gap-4"><button class="text-button" :disabled="loading || !d.online" @click="read('log')"><Icon name="refresh" :size="15" />Refresh log</button><button class="text-button" :disabled="loading || !d.online" @click="read('report')">View latest raw report<Icon name="code" :size="15" /></button></div><pre v-if="raw" class="code-block mt-4" aria-label="Latest raw USB report">{{ raw }}</pre><pre v-if="log" class="code-block mt-4" aria-label="Payload log">{{ log }}</pre><p class="caption mt-5">If the dashboard cannot open, retrieve /data/LibrePadPS5/LibrePadPS5.log through FTP and include your loader output.</p></section>
</template>
