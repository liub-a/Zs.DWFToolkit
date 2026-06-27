// Drives the WebAssembly DWF renderer (zs_dwf.js / zs_dwf.wasm). Loads a DWF/W2D
// into the module's in-memory filesystem, calls the C ABI, and shows the result.
'use strict';

const els = {
  drop: document.getElementById('drop'),
  file: document.getElementById('file'),
  info: document.getElementById('btnInfo'),
  pdf: document.getElementById('btnPdf'),
  dl: document.getElementById('dl'),
  rt: document.getElementById('rt'),
  log: document.getElementById('log'),
  view: document.getElementById('view'),
};

let mod = null;          // the Emscripten module
let inputBytes = null;   // currently loaded file
let inputName = null;

function log(msg) {
  els.log.textContent += msg + '\n';
  els.log.scrollTop = els.log.scrollHeight;
}

// EXPORT_NAME=ZsDwf, MODULARIZE=1 -> ZsDwf() returns a promise of the module.
ZsDwf().then((m) => {
  mod = m;
  els.rt.textContent = 'WebAssembly runtime ready.';
  log('runtime ready');
  refreshButtons();
}).catch((e) => { els.rt.textContent = 'runtime failed: ' + e; });

function refreshButtons() {
  const ok = !!(mod && inputBytes);
  els.info.disabled = !ok;
  els.pdf.disabled = !ok;
}

function loadFile(file) {
  const r = new FileReader();
  r.onload = () => {
    inputBytes = new Uint8Array(r.result);
    inputName = file.name;
    log(`loaded ${file.name} (${inputBytes.length} bytes)`);
    refreshButtons();
  };
  r.readAsArrayBuffer(file);
}

// --- C ABI helpers ---------------------------------------------------------
function writeInput() {
  const name = 'input' + (inputName && inputName.toLowerCase().endsWith('.w2d') ? '.w2d' : '.dwf');
  mod.FS.writeFile(name, inputBytes);
  return name;
}

function lastError() {
  return mod.ccall('zs_dwf_get_last_error', 'string', [], []);
}

function readInfo() {
  const inPath = writeInput();
  const cap = 1 << 20;
  const buf = mod._malloc(cap);
  const rc = mod.ccall('zs_dwf_get_info_json', 'number',
    ['string', 'number', 'number'], [inPath, buf, cap]);
  const json = mod.UTF8ToString(buf);
  mod._free(buf);
  if (rc !== 0) { log('info failed rc=' + rc + ' ' + lastError()); return; }
  try { log('info: ' + JSON.stringify(JSON.parse(json), null, 2)); }
  catch { log('info: ' + json); }
}

function renderPdf() {
  const inPath = writeInput();
  const out = 'out.pdf';
  log('rendering vector PDF…');
  const t0 = performance.now();
  const rc = mod.ccall('zs_dwf_render_dwf_pdf', 'number', ['string', 'string'], [inPath, out]);
  if (rc !== 0) { log('render failed rc=' + rc + ' ' + lastError()); return; }
  const bytes = mod.FS.readFile(out); // Uint8Array
  const ms = Math.round(performance.now() - t0);
  log(`vector PDF: ${(bytes.length / 1024).toFixed(0)} KB in ${ms} ms`);
  const url = URL.createObjectURL(new Blob([bytes], { type: 'application/pdf' }));
  els.view.src = url; els.view.hidden = false;
  els.dl.href = url;
  els.dl.download = (inputName || 'drawing').replace(/\.[^.]+$/, '') + '.pdf';
  els.dl.textContent = 'Download PDF';
  els.dl.hidden = false;
}

// --- UI wiring --------------------------------------------------------------
els.drop.addEventListener('click', () => els.file.click());
els.file.addEventListener('change', (e) => { if (e.target.files[0]) loadFile(e.target.files[0]); });
['dragover', 'dragenter'].forEach((ev) =>
  els.drop.addEventListener(ev, (e) => { e.preventDefault(); els.drop.classList.add('hover'); }));
['dragleave', 'drop'].forEach((ev) =>
  els.drop.addEventListener(ev, (e) => { e.preventDefault(); els.drop.classList.remove('hover'); }));
els.drop.addEventListener('drop', (e) => { if (e.dataTransfer.files[0]) loadFile(e.dataTransfer.files[0]); });
els.info.addEventListener('click', readInfo);
els.pdf.addEventListener('click', renderPdf);
