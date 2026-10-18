/* Binary firmware upload and setup visibility; no hardware or network. */
const assert = require("node:assert/strict");
const fs = require("node:fs");
const vm = require("node:vm");
const script = fs.readFileSync(require("node:path").join(__dirname,"../web/dashboard.js"),"utf8");
const nodes = Object.fromEntries(["xbox-setup","xbox-status","xbox-install","xbox-file","xbox-upload","message"]
  .map(id => [id,{hidden:false,dataset:{},files:[],value:"",textContent:"",disabled:false}]));
let status = {enabled:true,firmware:"missing"}, calls = [], success = true;
const context = vm.createContext({el:id => nodes[id],fetch:async (url,options) => {
  calls.push({url,options});
  return {ok:url === "/api/xbox" || success,json:async () => url === "/api/xbox" ? status : {message:success ? "Installed" : "Invalid firmware"}};
}});
vm.runInContext(script.slice(script.indexOf("async function refreshXbox()"),script.indexOf("async function act(")),context);
(async () => {
  await context.refreshXbox(); assert(!nodes["xbox-install"].hidden);
  status.firmware="bundled"; await context.refreshXbox(); assert(nodes["xbox-install"].hidden);
  assert.match(nodes["xbox-status"].textContent,/Firmware included/);
  context.showXboxInstall(); await context.refreshXbox(); assert(!nodes["xbox-install"].hidden);
  nodes["xbox-install"].dataset.opened="";
  status.firmware="installed"; await context.refreshXbox(); assert(nodes["xbox-install"].hidden);
  context.showXboxInstall(); await context.refreshXbox(); assert(!nodes["xbox-install"].hidden);
  await context.installXboxFirmware(); assert.equal(calls.filter(c=>c.url.endsWith("/firmware")).length,0);
  nodes["xbox-file"].files=[{size:1048577}]; await context.installXboxFirmware();
  assert.equal(calls.filter(c=>c.url.endsWith("/firmware")).length,0);
  const file={size:70620}; nodes["xbox-file"].files=[file];
  await context.installXboxFirmware();
  const upload=calls.find(c=>c.url.endsWith("/firmware"));
  assert.equal(upload.options.method,"POST"); assert.equal(upload.options.body,file);
  assert.equal(upload.options.headers["X-LibrePad-Control"],"1");
  assert.equal(upload.options.headers["Content-Type"],"application/octet-stream");
  assert.equal(nodes.message.textContent,"Installed"); assert(!nodes["xbox-upload"].disabled);
  assert(nodes["xbox-install"].hidden);
  context.showXboxInstall(); success=false; await context.installXboxFirmware();
  assert.equal(nodes.message.textContent,"Invalid firmware"); assert(!nodes["xbox-install"].hidden);
  status={enabled:false}; await context.refreshXbox(); assert(nodes["xbox-setup"].hidden);
  console.log("Xbox setup visibility, replacement, binary uploads, bounds and server errors passed");
})().catch(error=>{console.error(error);process.exitCode=1;});
