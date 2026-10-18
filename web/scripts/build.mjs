import { build } from 'vite';
import { createHash } from 'node:crypto';
import { readdir, readFile, writeFile } from 'node:fs/promises';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
const root = dirname(dirname(fileURLToPath(import.meta.url)));
const hash = data => createHash('sha256').update(data).digest('hex');
const canonical = (data, name) => /\.(?:js|vue|css|mjs|html|json|svg)$/.test(name)
  ? Buffer.from(data.toString('utf8').replace(/\r/g, '')) : data;
async function walk(folder) {
  const entries = await readdir(join(root, folder), { withFileTypes: true });
  const files = await Promise.all(entries.map(entry => entry.isDirectory()
    ? walk(folder + '/' + entry.name) : [folder + '/' + entry.name]));
  return files.flat().filter(file => !file.endsWith('.test.js'));
}
await build({ root });
const sources = [...await walk('src'), ...await walk('public'), 'index.html', 'package.json',
  'package-lock.json', 'vite.config.js', 'scripts/build.mjs'].sort();
const manifest = { sources: {}, assets: {} };
for (const source of sources) manifest.sources[source] = hash(canonical(await readFile(join(root, source)), source));
for (const asset of ['index.html', 'dashboard.css', 'dashboard.js', 'favicon.svg']) {
  const path = join(root, 'dist', asset);
  const data = canonical(await readFile(path), asset);
  await writeFile(path, data);
  manifest.assets[asset] = hash(data);
}
await writeFile(join(root, 'dist/source-manifest.json'), JSON.stringify(manifest, null, 2) + '\n');
console.log('Dashboard bundle and source manifest ready for offline payload builds.');
