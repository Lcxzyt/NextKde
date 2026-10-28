import assert from "node:assert/strict";
import {readFileSync} from "node:fs";
import vm from "node:vm";
const source = readFileSync(new URL("../../shell/desktop/modules/common/LiquidGlassPanel.qml", import.meta.url), "utf8");
const start = source.indexOf("    function updateContrastInk()");
const end = source.indexOf("\n    }", start) + "\n    }".length;
let restarts = 0;
const timer = {running: false, restart() { this.running = true; restarts++; }, stop() {this.running = false;}};
const state = {adaptiveInkReady: true, backdropLuminance: 0.9, darkForeground: false,
    _contrastInitialized: false, _pendingDarkForeground: false, contrastConfirm: timer,
    _contrastSide: -1, _contrastSideChanges: [], _contrastUnstableUntil: 0};
const update = vm.runInNewContext("(" + source.slice(start, end) + ")", state);
update(); assert.equal(state.darkForeground, true);
for (const value of [0.54, 0.5, 0.46, 0.45]) {
    state.backdropLuminance = value; update(); assert.equal(timer.running, false);
}
state.backdropLuminance = 0.1; update(); assert.equal(timer.running, true);
assert.equal(state.darkForeground, true); assert.equal(state._pendingDarkForeground, false);
const count = restarts;
update(); assert.equal(restarts, count, "stable candidate must not keep restarting confirmation");
state.backdropLuminance = 0.9; update(); assert.equal(timer.running, false, "short reversal cancels pending flip");
state.backdropLuminance = 0.1; update();
assert.ok(timer.interval >= 400, "rapid repeated reversal temporarily delays further flips");
state.darkForeground = state._pendingDarkForeground; timer.running = false;
for (const value of [0.46, 0.5, 0.54, 0.55]) {
    state.backdropLuminance = value; update(); assert.equal(timer.running, false);
}
state.backdropLuminance = 1; update(); assert.equal(timer.running, true);
state.adaptiveInkReady = false; update();
assert.equal(timer.running, false); assert.equal(state._contrastInitialized, false);
console.log("adaptive glass ink: hysteresis, candidate confirmation, cancellation and old-protocol fallback pass");
