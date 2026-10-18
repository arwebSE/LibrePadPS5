export async function request(path, options = {}) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), 8000);
  try {
    const response = await fetch('/api/' + path, {
      cache: 'no-store', ...options, signal: controller.signal,
      headers: options.method === 'POST'
        ? { 'X-LibrePad-Control': '1', ...options.headers } : options.headers,
    });
    const text = await response.text();
    let result;
    try { result = JSON.parse(text); } catch { result = text; }
    if (!response.ok) throw new Error(result.message || `Request failed (${response.status})`);
    return result;
  } catch (error) {
    if (error.name === 'AbortError') throw new Error('Console request timed out. Check your connection.');
    throw error;
  } finally { clearTimeout(timer); }
}

export function supportIssueUrl(state) {
  const url = new URL('https://github.com/arwebSE/LibrePadPS5/issues/new');
  url.searchParams.set('template', 'controller_problem.yml');
  if (state.version) url.searchParams.set('version', state.version);
  if (state.firmware) url.searchParams.set('firmware', state.firmware);
  return url.href;
}

export function eligibleUsers(users, pads, slot) {
  return users.filter(user => !user.protected && !user.native_connected &&
    !pads.some(pad => pad.slot !== slot && pad.live && pad.user === user.id));
}
