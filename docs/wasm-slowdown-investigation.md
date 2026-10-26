# Intermittent WebAssembly slowdown — investigation notes

Status: **cache defect corrected and regression verified; original Firefox trigger still unconfirmed**, 2026-10-27. The user authorised the SDL correction and a new Cloudflare deployment after the initial preloading-only release. Do not describe preloading as a fix for the gradual slowdown.

## Player observations

- Firefox on desktop can become extremely slow over time; restarting restores responsiveness. Other players have reported intermittent slowdowns too.
- Selecting portraits and opening panels in 整備 exposed delayed cursor/input response, but portraits are not established as the cause.
- The issue was initially associated with returning from battle. The latest reproduction happened while staying idle in 整備, possibly when the weather disappeared. A battle or repeated UI operations are therefore not established prerequisites.
- Firefox reproduction is intermittent: the problem disappeared, then returned. Edge did not reproduce it in the user's comparison. The native build did not show the problem.
- An earlier `-Os` release felt much slower. Current WASM compile and link settings use `-O3`; no controlled `-Os`/`-O3` benchmark established the magnitude or cause of that regression.

## What was checked, and what it does not establish

- Instrumented in-app browser runs covered dense preparation, portrait/panel selection, and battle → preparation. They did not reproduce the reported sustained degradation. These runs are not Firefox verification.
- The original frame loop can omit its browser yield when over budget; `Engine::delay(0)` also skips sleeping. A focused Asyncify test confirmed the zero-delay behavior. This can amplify input starvation **after frames become expensive**; it does not identify what makes them progressively expensive. The experimental scheduling change was removed from the retained changes.
- The texture manager retains loaded groups. Battle terrain also uses large generated render targets. At 64 tiles and the existing extension margin, the requested terrain sizes are 13,824 × 6,912 and 13,824 × 13,824. This policy already exists in the earlier inspected source revision, so it alone does not establish a recent regression.
- WebGL instrumentation counted texture objects and image upload dimensions. Multiplying dimensions by four estimates RGBA storage; it is **not measured process RAM, physical VRAM, or proof of memory pressure**. Native process RAM and those calculated texture extents are different measurements. No memory-pressure cause was established.
- Experimental texture cleanup and the opt-in timing overlay were removed. Neither was established as the Firefox root-cause fix.
- A hidden ImGui input queue was considered: the frame setup still runs when its UI is hidden. No accumulating queue was demonstrated in the observed runs.
- Firefox desktop automation was unavailable, but the user subsequently supplied a Firefox performance recording captured during a lagging battle. Its findings supersede the earlier hypotheses below.

## Supplied Firefox battle profile

Analysed `Firefox 2026-10-26 23.31 profile.json.gz` locally; do not upload this browser profile. The recording covers approximately 92.27 seconds. The game content thread is `GeckoMain`, PID 51992 / TID 69604.

- **38,672 of 47,238 game-thread samples (81.9%)** contain `PWebGL::Msg_GetLinkResult`, primarily waiting in `ZwWaitForAlertByThreadId`. This is sampled wall time including waits, not 82% CPU utilisation.
- The resolved stack is `SDL_RenderPresent` (also render-target flushes) → `GLES2_RunCommandQueue` → `GLES2_SelectProgram` → `emscripten_glGetProgramiv` → Firefox `getProgramParameter` → synchronous `GetLinkResult` IPC.
- There are 39,066 marker endpoints, not that many distinct calls: 19,532 complete start/end pairs, plus one unmatched endpoint at each recording boundary. Complete waits total **75.28 seconds**; median **3.95 ms**, 95th percentile **4.74 ms**, maximum **36.19 ms**. The first six ten-second buckets each contain roughly 8.4–8.8 seconds of these waits. This is sustained, not just initial loading.
- GC on the game thread: **20 major collections**, 130 incremental slices, approximately **125.4 ms total slice pauses**, maximum slice **6.001 ms**. Twenty minor collections total **12.61 ms**, maximum **1.10 ms**; minor pauses can overlap major slices and should not simply be added. Major/minor collection starts are about 4.8 seconds apart at the median. This thread does not show a full collection every 0.2 seconds.
- A major collection's enclosing marker can span hundreds of milliseconds between slices. Use its `total_time`/`max_pause` or individual slice pauses, not enclosing elapsed duration, to assess GC stalls.

This trace establishes synchronous WebGL program-result waits as the dominant stall in this capture. It does not measure GPU shader compilation alone: the synchronous query can also wait for queued rendering work and IPC processing. It does not establish memory pressure, weather, or compiler optimisation as the original trigger.

## Shader caching findings before the correction

The pre-fix WASM dependency was SDL **3.4.2#2**, built using the project's **patched overlay port**, not stock SDL. `wasm/vcpkg-configuration.json` selects `android/ports`, whose SDL port applies `fix-gles2-program-cache-tail.patch`. The installed ABI/SPDX records contain that patch's matching SHA-256 (`fc6305e7…d826330`), and the installed release archive matches both its recorded SHA-256 (`ad2f18e3…77f4b62`) and the buildtree archive. The WASM link command searches this installation and explicitly includes its `libSDL3.a`. Its local build cache identifies source `D:/projects/vcpkg/buildtrees/sdl3/src/ease-3.4.2-03000b87f9.clean`. Inspect `src/render/opengles2/SDL_render_gles2.c`, especially `GLES2_CacheProgram` and `GLES2_SelectProgram`.

- SDL caches compiled shaders and up to **eight linked programs**, keyed by vertex/fragment shader IDs. A cache hit returns without a `GL_LINK_STATUS` query. The single `glGetProgramiv` call in this renderer is immediately after linking a newly created program. The observed stack therefore strongly indicates repeated program creation/cache misses, rather than image files being downloaded repeatedly.
- Firefox also caches link results: its `GetLinkResult` sends synchronous IPC when the program result is pending. `LinkProgram` invalidates that result. See [SDL 3.4.2 renderer](https://github.com/libsdl-org/SDL/blob/release-3.4.2/src/render/opengles2/SDL_render_gles2.c) and [Mozilla implementation](https://github.com/mozilla-firefox/firefox/blob/main/dom/canvas/ClientWebGLContext.cpp). Mozilla main is explanatory source, not the exact build ID from the capture.
- The existing project patch clears the new tail's `next` pointer after eviction, preventing a link to the freed entry. That fix is present in the installed source/archive provenance.
- A separate SDL list defect remains in the patched source: promoting the tail entry to the head on a cache hit does not update `program_cache.tail`. A later eviction uses that stale tail and can truncate the reachable list. Reproducing the patched source's list operations with requests `1…8`, then `1`, then `9` leaves `count == 8` but only program `9` reachable. Subsequent requests for `2`, `3`, `4` each replace the sole reachable entry. This demonstrates a cache failure mechanism, **not that the affected game session reached the ninth shader variant**. Do not diagnose this as failure to select the project's patched SDL port.
- A temporary WebGL API counter on the fresh preloading-only build in the Chromium-based in-app browser observed five linked programs in preparation, increasing to seven across a completed battle. Each program had one link-status and one active-uniform query (14 queries total), with no program deletion. Program switches continued while link/query counts stayed fixed. This confirms normal caching in that comparison; it does not reproduce or disprove the Firefox failure.

The next targeted capture should count `createProgram`, `linkProgram`, `deleteProgram`, and `getProgramParameter`, recording program IDs and attached shader IDs. Distinguish cache eviction/list corruption from renderer/context recreation or failed links. Check cache reachability against its count and actual shader variants when slowdown begins. Compare a correction to the tail bookkeeping against the same affected sequence before shipping it. No renderer dependency, cache size, or compiler setting is changed in the retained preloading work.

## Edge comparison supplied afterward

Analysed `download.devtools` locally as a gzip-compressed Chromium trace. It contains 408,017 trace events and 91,626 CPU-profile samples. The game renderer main thread is PID 8504 / TID 27096; the CPU sample stream spans **33.92 seconds**.

- Main-thread `RunTask` events total **6.01 seconds** over 33.94 seconds of trace time (approximately **17.7% busy**). The weighted CPU sample stream attributes **26.76 seconds / 78.9% to idle**. Task and CPU-sampling measurements are different and should not be expected to match exactly.
- Sampled inclusive time under `GLES2_RunCommandQueue` is approximately **355 ms** (1.05% of the sample interval); under `GLES2_SelectProgram`, **83.5 ms** (0.25%). No sampled function node names `glGetProgramiv`, `getProgramParameter`, or `linkProgram`. Absence from a sampling profile is not proof of zero calls.
- `RunNode::present` has approximately 2.09 seconds of inclusive sampled time, but includes render-target operations and Asyncify/browser scheduling. Do not describe the whole span as GPU rendering work. `checkFramebufferStatus` contributes approximately 954 ms of sampled self time, a separate synchronization cost worth tracking after the dominant Firefox problem is resolved.
- Thirty minor collections total **6.17 ms**, maximum **0.307 ms**; median spacing is **1.20 seconds**. There is no major-GC event in the main-thread trace. Individual tasks still reach 405 ms and 342 ms, so the capture is not entirely free of stalls.
- Selected WASM function offsets match Firefox: `GLES2_SelectProgram` at 13,270,235 and `SDL_RenderPresent` at 13,209,929. This is consistent with the same renderer binary; it is not a complete binary identity check.

The Edge session is substantially healthier than the affected Firefox session. The two captures have different durations, scenes/actions, sampling systems, and browser internals; they are not a controlled browser throughput benchmark. In particular, they cannot distinguish fewer program queries in Edge from shorter queries solely from sampled stacks. They do support prioritizing the abnormal repeated program-result wait path in Firefox over a general battle-logic or GC explanation.

## Earlier preloading-only deployment

Cloudflare production was updated to version **0.2.21**, deployment `901d425a`, at <https://kyschess.pages.dev/kyschess>. Packaged JS and compressed WASM match the served production bytes. The WASM was checked for absence of temporary performance probes. `docs/更新日志.md` equals HEAD, and packaged `config/changelog.md` matches it byte for byte. The scheduler, renderer cache, texture-residency policy, and optimisation flags retain their original implementation. This deployment is not represented as a fix for the intermittent Firefox stall.

## Weather lead for the next reproduction

Inspect [Weather.cpp](../src/Weather.cpp), [ParticleSystem.cpp](../src/ParticleSystem.cpp), [ParticleExample.cpp](../src/ParticleExample.cpp), [MainScene.cpp](../src/MainScene.cpp), and scene ownership in [RunNode.cpp](../src/RunNode.cpp).

- `Weather::draw()` draws and updates the shared particle system. `Weather::backRun()` is empty.
- `MainScene::backRun()` updates cloud coverage and calls `Weather::setWeather()`; a zero rain count stops emission. Existing particles then expire through the draw/update path.
- Rain and snow have bounded particle capacities (1,000 and 700 respectively). This code inspection did not demonstrate unbounded particle growth.
- Weather is a shared scene node attached to both subscenes and battles. Verify actual update/draw frequency and ownership across overlays and battle transitions; do not infer a leak merely from the shared instance.

On an affected Firefox session, record idle 整備 before, during, and after the weather transition, including the first visible slowdown. Compare the same state in Edge. Capture a Firefox performance recording and distinguish time spent in input dispatch, scene/UI work, SDL/WebGL submission, browser GC/runtime work, and scheduling gaps. Correlate weather active state, particle count, and update/draw frequency with the slowdown. Repeat without a battle to test whether battle history is necessary.

Do not ship a weather, rendering, cache, compiler, or frame-loop change as the root-cause fix without that evidence. Preserve the existing 更新日志; the user explicitly requested no edits to it.

## Supplied Firefox console output

The supplied console capture contains no full-frame timings or weather transition timestamp. Most `[perf-ui]` samples are 0–1 ms and preparation rebuilds are usually 1–4 ms; isolated samples reach 94 ms for UI work and 38 ms for rebuilding. These samples neither explain nor rule out sustained input/renderer/browser delays.

The `MaxListenersExceededWarning` stacks originate in `moz-extension://…/scripts/contentscript.js`; other messages name `metamask-multichain-provider` and `background-liveness`. These are extension-side messages, not a demonstrated game EventEmitter leak. Compare with extensions disabled or a clean Firefox profile when reproducing; do not blame an extension without that comparison.

The capture also includes WebGL lazy-initialization warnings, missing texture messages, audio autoplay/instrument warnings, and the existing SDL/Emscripten main-loop timing warning. None establishes the gradual slowdown's cause in isolation.

Temporary `[perf-ui]`/`[perf-rebuild]` probes were present in a served intermediate build despite being removed from source. Restoring a file with an older timestamp can leave an incremental build's newer object file untouched. The final preloading-only build must recompile those files and be checked for diagnostic markers in the actual WASM, not only in source.

## Retained implementation scope

The initial retained scope was the image preloading/loading UI work and this note: shared lazy asset requests, background portrait loading with pending/error placeholders, one-time shared UI asset preloading, and visible scene/battle resource-loading progress. Existing unrelated working-tree changes belong to the user and are preserved.


## Authorised SDL correction, 2026-10-27

The user requested fixing the remaining cache defect, rebuilding WASM, and deploying to Cloudflare. The existing overlay patch now updates `program_cache.tail = entry->prev` when a cache hit moves the tail entry to the head. Its existing eviction cleanup remains. The overlay port revision is **3.4.2#3** so dependency installation records and rebuilds the changed package.

Verification compiled the actual `GLES2_CacheProgram` function extracted from the old and rebuilt SDL source into WASM, with mock GL calls counting program creation/deletion and an independent LRU model. The old function failed on tail reuse. The corrected function passed 100,000 switches among eight resident programs with no additional links, 20,000 mixed requests with exact LRU ordering/link-count agreement, and complete program/allocation cleanup. This verifies the list correction; it is not a measurement of Firefox rendering latency.

`wasm/rebuild.ps1` rebuilt SDL and its dependent WASM libraries and relinked the game. Installed package metadata records the changed overlay patch hash (`bc1f8a43…c4889fa`); the release SDL archive hash is `593e1c37…7497df3`. Native Release build, 1,223 native test cases / 28,956 assertions, 67 Python tests, and nine JavaScript loader tests passed. The packaged WASM matches the new build, contains no temporary performance probes, and the changelog remains byte-identical to its pre-fix contents. Game version stays **0.2.21**. No battle rules, configuration, compiler flags, or scheduling changes are part of this correction.

Cloudflare production deployment **`2bd0032b`** serves this correction at <https://kyschess.pages.dev/kyschess>. Production JS and compressed WASM were downloaded and matched against the rebuilt package. The local browser smoke test resumed the synthetic preparation save, opened a portrait inspector, loaded battle resources, entered formation, and completed the battle. Original intermittent Firefox reproduction remains to be retested in an affected session; do not claim that all gradual slowdowns are resolved from the cache regression test alone.
