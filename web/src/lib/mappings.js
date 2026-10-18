export const targets = ['none', 'cross', 'circle', 'square', 'triangle', 'l1', 'r1', 'l2', 'r2',
  'create', 'options', 'l3', 'r3', 'ps', 'touchpad', 'up', 'right', 'down', 'left'];
export const defaultMap = ['triangle', 'circle', 'cross', 'square', 'l1', 'r1', 'l2', 'r2',
  'create', 'options', 'l3', 'r3', 'ps', 'touchpad'];
export const fields = [
  ['Report bytes', 8, 1, 64], ['Report ID (-1: none)', -1, -1, 255],
  ['Hat byte (-1: none)', 2, -1, 63], ['Neutral hat', 15, 8, 15],
  ['Left X byte', 3, -1, 63], ['Left Y byte', 4, -1, 63],
  ['Right X byte', 5, -1, 63], ['Right Y byte', 6, -1, 63],
];
export const drivers = [
  ['switch8', '8-byte wired preset'], ['driver:ds4', 'DualShock 4 USB'],
  ['driver:ds3', 'DualShock 3 USB'], ['driver:xinput', 'Xbox / XInput USB'],
  ['driver:switchpro', 'Switch Pro USB'], ['driver:8bitdo', '8BitDo USB adapter'],
  ['driver:generic', 'Generic USB HID'], ['driver:bluetooth', 'Bluetooth button mapping'],
  ['custom', 'Custom raw USB report'],
];
export const profileRows = text => text.split('\n').map(row => row.trim()).filter(row => row && !row.startsWith('#'));
export function stageRow(text, vid, pid, layout, mapping, bits) {
  if (![vid, pid].every(id => /^[0-9a-f]{4}$/i.test(id) && parseInt(id, 16)))
    throw new Error('Use four hexadecimal digits for each VID and PID.');
  const id = `${vid}:${pid}`.toLowerCase();
  const rows = text.split('\n').filter(row => row.trim() && !row.toLowerCase().startsWith(id + ' '));
  rows.push(`${id} ${layout} ${mapping.map((target, i) => target + (layout.startsWith('hid:') ? '@' + bits[i] : '')).join(',')}`);
  if (profileRows(rows.join('\n')).length > 8) throw new Error('You can save up to eight controller profiles.');
  const result = rows.join('\n') + '\n';
  if (result.length >= 2048) throw new Error('Profiles too large (maximum 2047 characters).');
  return result;
}
