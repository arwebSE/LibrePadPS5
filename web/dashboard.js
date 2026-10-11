let stopped = false;
const el = (id) => document.getElementById(id);
const dashboardTabs = Array.from(document.querySelectorAll('[role="tab"]'));
function showPanel(tab) {
  dashboardTabs.forEach((item) => {
    const selected = item === tab;
    item.setAttribute("aria-selected", String(selected));
    item.tabIndex = selected ? 0 : -1;
    el(item.getAttribute("aria-controls")).hidden = !selected;
  });
}
dashboardTabs.forEach((tab, index) => {
  tab.addEventListener("click", () => showPanel(tab));
  tab.addEventListener("keydown", (event) => {
    const next = {
      ArrowRight: (index + 1) % dashboardTabs.length,
      ArrowLeft: (index + dashboardTabs.length - 1) % dashboardTabs.length,
      Home: 0,
      End: dashboardTabs.length - 1,
    }[event.key];
    if (next === undefined) return;
    event.preventDefault();
    showPanel(dashboardTabs[next]);
    dashboardTabs[next].focus();
  });
});
function updateOptions(id, options, selected) {
  const select = el(id);
  const signature = JSON.stringify(options);
  if (select.dataset.options !== signature) {
    select.replaceChildren();
    for (const item of options) {
      const option = document.createElement("option");
      option.value = item.value; option.textContent = item.text; option.disabled = !!item.disabled;
      select.append(option);
    }
    select.dataset.options = signature;
  }
  select.value = selected;
}
let nativeProfilesSignature = "";
async function refresh() {
  if (stopped) return;
  try {
    let r = await fetch("/api/state", { cache: "no-store" }),
      s = await r.json();
    const pads = s.slots || [{...s, slot: 0, name: "Gamepad"}];
    let selectedPad = Number(el("pads").value || 0);
    updateOptions("pads", pads.map(pad => ({value:pad.slot,
      text:"Pad " + (pad.slot + 1) + ": " + (pad.name || "Empty") + (pad.live ? " (active)" : pad.usb_connected ? " (connected)" : "")})), selectedPad);
    const users = s.users;
    s = {...s, ...(pads.find(pad => pad.slot === selectedPad) || pads[0]), users};
    el("pad-summary").textContent = pads.filter(pad => pad.usb_connected).length + " gamepad(s) connected. " + pads.filter(pad => pad.live).length + " virtual pad(s) active.";
    const nativeSignature = JSON.stringify(users.map(user => [user.id,user.name,user.protected,user.primary_protected]));
    if (nativeSignature !== nativeProfilesSignature) {
      nativeProfilesSignature = nativeSignature;
      el("native-profiles").replaceChildren();
      for (const user of users) {
        const label = document.createElement("label"), checkbox = document.createElement("input");
        checkbox.type = "checkbox"; checkbox.checked = user.protected;
        checkbox.disabled = user.primary_protected;
        checkbox.addEventListener("change", () => act("protect?user=" + user.id + "&reserved=" + (checkbox.checked ? 1 : 0)));
        label.append(checkbox, " Reserve " + user.name + " for a native DualSense");
        el("native-profiles").append(label);
      }
    }
    const bt = await (await fetch("/api/bluetooth", { cache: "no-store" })).json();
    el("bt-status").textContent = bt.message + (bt.pairing ? " · " + bt.pairing + " seconds remaining" : "") +
      (bt.connected ? " · USB ID " + bt.vid.toString(16).padStart(4,"0") + ":" + bt.pid.toString(16).padStart(4,"0") : "");
    el("fw").textContent = (s.version ? "· " + s.version + " " : "") + "· FW " + s.firmware;
    el("status").textContent = s.usb_connected
      ? s.live
        ? "Controller active"
        : "Controller connected · virtual pad released or unavailable"
      : "Controller unplugged";
    el("status").className = s.live ? "ok" : "";
    let u = s.users.find((u) => u.id === s.user);
    el("profile").textContent = s.live
      ? s.user === "ffffffff"
        ? "Unassigned: press Home and choose the separate controller profile."
        : "Assigned to " + (u ? u.name : s.user)
      : "Select Reconnect to create the virtual pad.";
    el("input").textContent =
      "L stick " +
      s.axes[0] +
      ", " +
      s.axes[1] +
      " · R stick " +
      s.axes[2] +
      ", " +
      s.axes[3] +
      " · Buttons 0x" +
      s.buttons.toString(16);
    let selected = el("users").value;
    const occupied = id => pads.some(pad => pad.slot !== selectedPad && pad.live && pad.user === id);
    const eligible = user => !user.protected && !occupied(user.id);
    if (!s.users.some(user => user.id === selected && eligible(user)))
      selected = s.users.find(user => user.id === s.user && eligible(user))?.id || s.users.find(eligible)?.id || "";
    updateOptions("users", s.users.map(user => ({value:user.id,
      text:user.name + (user.foreground ? " (active)" : "") + (user.protected ? " (reserved for DualSense)" : occupied(user.id) ? " (assigned to another gamepad)" : ""),
      disabled:!eligible(user)})), selected);
    el("home").disabled = !s.live;
    let target = s.users.find((u) => u.id === el("users").value);
    el("bind").disabled =
      !s.live || !target || target.protected || target.native_connected || pads.some(pad => pad.slot !== selectedPad && pad.live && pad.user === target.id);
    el("reconnect").disabled =
      !s.usb_connected || s.status === 1 || s.status === 2 || s.live;
    let native = s.users.filter((u) => u.native_connected === true),
      unknown = s.users.some((u) => u.native_connected === null);
    el("native").textContent =
      (native.length
        ? "Connected: " + native.map((u) => u.name).join(", ")
        : unknown
          ? "Native connection status unavailable from this payload process."
          : "No connected native controller reported.") +
      " Reserved profiles: " +
      (s.users.filter((u) => u.protected).map(u => u.name).join(", ") || "not configured");
  } catch (e) {
    el("status").textContent = "Dashboard offline";
  }
}
function padAct(action, query = "") {
  return act(action + "?slot=" + el("pads").value + (query ? "&" + query : ""));
}
async function act(path) {
  try {
    let r = await fetch("/api/" + path, {
      method: "POST",
      headers: { "X-FreePad-Control": "1" },
    });
    let j = await r.json();
    el("message").textContent = j.message;
    if (path === "stop" && r.ok) {
      stopped = true;
      el("status").textContent = "Payload stopped";
      return;
    }
    await refresh();
  } catch (e) {
    el("message").textContent = "Request failed: " + e.message;
  }
}
async function logs() {
  try {
    el("log").textContent = await (
      await fetch("/api/log", { cache: "no-store" })
    ).text();
  } catch (e) {
    el("message").textContent = e.message;
  }
}
function pairBluetooth() {
  if (!el("bt-risk").checked) {
    el("message").textContent = "Read and acknowledge the Bluetooth warning first."; return;
  }
  const address=el("bt-address").value.trim();
  if (!/^(?:[0-9a-f]{2}:){5}[0-9a-f]{2}$/i.test(address)) {
    el("message").textContent = "Use an address like AA:BB:CC:DD:EE:FF."; return;
  }
  act("bluetooth/pair?address=" + address);
}
const sources = Array.from({ length: 14 }, (_, i) => "Input " + (i + 1));
const targets = [
  "none",
  "cross",
  "circle",
  "square",
  "triangle",
  "l1",
  "r1",
  "l2",
  "r2",
  "create",
  "options",
  "l3",
  "r3",
  "ps",
  "touchpad",
  "up",
  "right",
  "down",
  "left",
];
const defaultMap = [
  "triangle",
  "circle",
  "cross",
  "square",
  "l1",
  "r1",
  "l2",
  "r2",
  "create",
  "options",
  "l3",
  "r3",
  "ps",
  "touchpad",
];
const layoutFields = [
  ["length", "Report bytes", 8, 1, 64],
  ["reportid", "Report ID (-1: none)", -1, -1, 255],
  ["hat", "Hat byte (-1: none)", 2, -1, 63],
  ["neutral", "Neutral hat value", 15, 8, 15],
  ["lx", "Left X byte", 3, -1, 63],
  ["ly", "Left Y byte", 4, -1, 63],
  ["rx", "Right X byte", 5, -1, 63],
  ["ry", "Right Y byte", 6, -1, 63],
];
for (const [id, label, value, min, max] of layoutFields) {
  const wrapper = document.createElement("label");
  wrapper.textContent = label;
  const input = document.createElement("input");
  Object.assign(input, { id: "layout-" + id, type: "number", value, min, max });
  wrapper.append(input);
  el("layoutfields").append(wrapper);
}
sources.forEach((name, i) => {
  const label = document.createElement("label");
  label.id="source" + i;
  label.textContent = name + " · source bit ";
  const bit = document.createElement("input");
  Object.assign(bit, {
    id: "bit" + i,
    type: "number",
    min: 0,
    max: 511,
    value: i,
  });
  const select = document.createElement("select");
  select.id = "map" + i;
  targets.forEach((target) => select.add(new Option(target, target)));
  label.append(bit, " → ", select);
  el("maps").append(label);
});
function defaults() {
  defaultMap.forEach((target, i) => {
    el("map" + i).value = target;
    el("bit" + i).value = i;
  });
  layoutFields.forEach(([id, , value]) => (el("layout-" + id).value = value));
  el("layout").value = "switch8";
  selectDriver("switch8");
}
function selectDriver(value) {
  if (value==="custom") {
    value="hid:" + layoutFields.map(([id]) => el("layout-" + id).value).join(":");
  }
  el("driver").value=value.startsWith("hid:") ? "custom" : value;
  el("layout").value=value;
  const named=value.startsWith("driver:");
  sources.forEach((name,i)=>{
    el("source"+i).firstChild.nodeValue=(named ? defaultMap[i] : name) + (named ? " input " : " · source bit ");
    el("bit"+i).disabled=named;
  });
}
function customLayout() {
  el("layout").value =
    "hid:" + layoutFields.map(([id]) => el("layout-" + id).value).join(":");
  selectDriver(el("layout").value);
}
function editProfile() {
  const [ids, layout, map] = el("controllers").value.split(" ");
  if (!map) return;
  [el("vid").value, el("pid").value] = ids.split(":");
  el("layout").value = layout;
  selectDriver(layout);
  const fields =
    layout === "switch8"
      ? layoutFields.map(([, , value]) => value)
      : layout.startsWith("hid:") ? layout.split(":").slice(1) : layoutFields.map(([, , value])=>value);
  layoutFields.forEach(([id], i) => (el("layout-" + id).value = fields[i]));
  map.split(",").forEach((entry, i) => {
    const [target, bit] = entry.split("@");
    el("map" + i).value = target;
    el("bit" + i).value = bit ?? i;
  });
}
async function loadProfiles() {
  try {
    const response = await fetch("/api/profiles", { cache: "no-store" });
    if (!response.ok) throw Error("Load failed");
    const text = await response.text();
    el("profiletext").value = text;
    el("controllers").replaceChildren();
    text
      .trim()
      .split("\n")
      .filter((row) => row && !row.startsWith("#"))
      .forEach((row) => {
        const [ids, layout] = row.split(" ");
        el("controllers").add(
          new Option(
            ids +
              " · " +
              (layout === "switch8" ? "8-byte preset" : layout.startsWith("driver:") ? layout.slice(7) : "Custom HID"),
            row,
          ),
        );
      });
    editProfile();
  } catch (error) {
    el("message").textContent = error.message;
  }
}
function stageProfile() {
  const vid = el("vid").value.toLowerCase(),
    pid = el("pid").value.toLowerCase();
  if (!/^[0-9a-f]{4}$/.test(vid) || !/^[0-9a-f]{4}$/.test(pid)) {
    el("message").textContent = "Use four hexadecimal digits for each ID";
    return;
  }
  const layout = el("layout").value;
  const custom = layout.startsWith("hid:");
  const map = sources.map(
    (_, i) => el("map" + i).value + (custom ? "@" + el("bit" + i).value : ""),
  );
  const id = vid + ":" + pid,
    row = id + " " + layout + " " + map.join(",");
  const rows = el("profiletext")
    .value.split("\n")
    .filter((row) => row.trim() && !row.startsWith(id + " "));
  rows.push(row);
  el("profiletext").value = rows.join("\n") + "\n";
  el("message").textContent =
    "Staged in editor. Release pad, then Save & apply.";
}
async function saveProfiles() {
  try {
    let text = el("profiletext").value;
    if (text.length >= 2048) throw Error("Profiles too large");
    let r = await fetch("/api/profiles?data=" + encodeURIComponent(text), {
        method: "POST",
        headers: { "X-FreePad-Control": "1" },
      }),
      j = await r.json();
    el("message").textContent = j.message;
    if (r.ok) await loadProfiles();
  } catch (e) {
    el("message").textContent = e.message;
  }
}
function exportProfiles() {
  let url = URL.createObjectURL(
      new Blob([el("profiletext").value], { type: "text/plain" }),
    ),
    a = document.createElement("a");
  a.href = url;
  a.download = "controllers.conf";
  a.click();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}
async function importProfiles(input) {
  try {
    if (!input.files[0]) return;
    if (input.files[0].size >= 2048) throw Error("Profiles too large");
    el("profiletext").value = await input.files[0].text();
    el("message").textContent = "Imported to editor; not saved yet.";
  } catch (e) {
    el("message").textContent = e.message;
  }
}
async function rawReport() {
  try {
    el("rawreport").textContent = await (
      await fetch("/api/report", { cache: "no-store" })
    ).text();
  } catch (e) {
    el("message").textContent = e.message;
  }
}
defaults();
loadProfiles();
refresh();
setInterval(refresh, 1000);
