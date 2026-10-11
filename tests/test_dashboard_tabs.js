/* Tab navigation only, with a disposable DOM stub and no network/console APIs. */
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");
const script = fs.readFileSync(path.join(__dirname, "../web/dashboard.js"), "utf8");
const end = script.indexOf("async function refresh()");
assert(end > 0);
const panels = {
  "panel-controller": { hidden: false },
  "panel-advanced": { hidden: true },
};
let focused;
const tabs = ["controller", "advanced"].map((name, index) => ({
  attributes: { "aria-controls": "panel-" + name, "aria-selected": String(index === 0) },
  tabIndex: index === 0 ? 0 : -1,
  handlers: {},
  getAttribute(name) { return this.attributes[name]; },
  setAttribute(name, value) { this.attributes[name] = value; },
  addEventListener(name, handler) { this.handlers[name] = handler; },
  focus() { focused = this; },
}));
vm.runInNewContext(script.slice(0, end), {
  document: { querySelectorAll: () => tabs, getElementById: (id) => panels[id] },
});
function check(selected) {
  tabs.forEach((tab, index) => {
    assert.equal(tab.attributes["aria-selected"], String(index === selected));
    assert.equal(tab.tabIndex, index === selected ? 0 : -1);
    assert.equal(panels[tab.attributes["aria-controls"]].hidden, index !== selected);
  });
}
function key(index, name, expected) {
  let prevented = false;
  tabs[index].handlers.keydown({ key: name, preventDefault() { prevented = true; } });
  assert(prevented);
  check(expected);
  assert.equal(focused, tabs[expected]);
}
check(0);
tabs[1].handlers.click(); check(1);
tabs[0].handlers.click(); check(0);
key(0, "ArrowRight", 1);
key(1, "ArrowRight", 0);
key(0, "ArrowLeft", 1);
key(1, "Home", 0);
key(0, "End", 1);
tabs[1].handlers.keydown({ key: "Tab", preventDefault() { assert.fail("Do not trap Tab"); } });
check(1);
console.log("Dashboard tab selection, visibility and keyboard tests passed");
