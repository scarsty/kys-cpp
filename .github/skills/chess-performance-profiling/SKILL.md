---
name: chess-performance-profiling
description: Benchmark and profile KYS Chess campaign replay verification on Windows using the Release CLI, Visual Studio Diagnostics, and scripted CPU stack reports. Use when a campaign replay is available to investigate battle performance or measure optimization gains.
---

# Chess performance profiling

Run commands from the repository root in a PowerShell 7 terminal, including VS Code's terminal. Use [Profile-ChessReplay.ps1](../../../tools/Profile-ChessReplay.ps1) to benchmark and capture, and [summarize_cpu_stacks.py](../../../tools/summarize_cpu_stacks.py) to read exported reports. The workflow requires Visual Studio Diagnostics, Windows Performance Toolkit's `xperf.exe`, and Python. Diagnostics performs the capture; `xperf` exports the collected stacks. No GUI automation or separate tracing capture is needed.

## Benchmark and capture

Set `$replayPath` to the supplied campaign replay. If comparing changes, preserve the baseline executable, matching PDB, and runtime DLLs before rebuilding.

```powershell
.\.github\build-command.ps1 -Configuration Release -Target kys_chess_cli
pwsh -NoProfile -File .\tools\Profile-ChessReplay.ps1 -Replay $replayPath
```

The script runs `verify` with the repository as the working directory, `work/game-dev` as the data root, and top-level `config` as the config root. It performs one warm-up and seven unprofiled validations, then captures a separate run with Visual Studio Diagnostics' `CpuUsageHigh.json`.

Use `-Runs` to change the measured run count, `-CliPath` to select another executable, and `-XperfPath` to select another exporter. Keep the matching PDB beside the executable. The default output prefix is timestamped; use a fresh prefix when supplying `-OutputPrefix` because capture/extraction directories cannot be reused.

Outputs under `output/profiles`:

- `*-benchmark.json`: individual wall/CPU times, median wall time, launch arguments, working directory, binary/PDB/replay hashes, and Git state.
- `*.diagsession`: the Diagnostics capture.
- `*-stacks.md` and `*-stacks.json`: summarized CPU hotspots.
- `*-stacks.html`: complete caller/callee and butterfly tables.

Use only unprofiled runs for timing. The benchmark includes startup, content loading, replay decisions, battle execution, and verification. Each measured run must exit zero and end with `重播驗證成功`.

## Read hotspots

Open `*-stacks.md` in VS Code Markdown preview, or inspect `*-stacks.json` with scripts. Start with self costs, then follow inclusive paths and HTML caller tables to identify the battle operations responsible.

To regenerate summaries from an exported report:

```powershell
python .\tools\summarize_cpu_stacks.py $stackReportPath
```

Inclusive percentages overlap; allocator samples measure CPU spent allocating/freeing, not allocation counts or bytes. Optimized/inlined functions may be attributed to their enclosing function. Check sample accounting: summed self samples should equal the process total, and self samples must not exceed inclusive samples. The exporter writes literal C++ template brackets, which the existing parser preserves.

## Compare optimizations

Warm up both executables, then run baseline and candidate serially in alternating order. Keep the replay, CLI operation, data/config roots, working directory, and output redirection identical. Avoid concurrent builds, tests, or profiling captures while measuring.

Require successful replay validation for every run. Record individual times and executable/PDB/replay hashes; compare medians, variability, and pairs won. Report timing separately from sampled CPU percentages, and tie hotspots to source locations before choosing changes.
