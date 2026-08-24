# Chess Effect Schema C++ Generation and Build Integration Plan

## Purpose

Replace `tools/generate_chess_effect_schemas.py` as the source of generated chess-effect schemas. The replacement reads the real compiled C++ authoring descriptors and produces the four checked-in JSON Schema files used by the YAML editor.

This plan implements the schema portion of [效果規則通用簡式與舊語法移除設計](效果規則通用簡式與舊語法移除設計.md). It does not change the authoring syntax, parser semantics, or runtime validation contract.

The required end state is:

- Compiled C++ descriptors are the only source of truth for effect authoring names, fields, shapes, requiredness, nested payloads, enum labels, and dynamic-key classes.
- A small host-only C++ executable renders JSON Schema directly from those descriptor objects.
- Every relevant native developer build generates schemas into its intermediate directory and compares them with the checked-in schemas.
- Ordinary builds never modify the source tree.
- A separate explicit target updates all four checked-in schemas.
- Android and WebAssembly builds do not try to execute a target-platform generator.
- No code reads or parses C++ source text to discover schema metadata.

## Non-goals

The schema generator is not a headless game or config checker.

- It does not accept a chess YAML file as input.
- It does not invoke `ChessContentLoader`, the effect parser, or runtime validators.
- It does not duplicate cross-field or gameplay invariants in a second executable.
- It does not replace starting the game as the authoritative runtime content check.
- It does not make Android or WebAssembly build hosts execute cross-compiled binaries.

JSON Schema remains editor support for completion and structural diagnostics. The normal game loader remains authoritative for duplicate raw YAML keys, loader-specific restrictions, runtime capability checks, references between files, exact-runtime action composition, and other semantic rules that are intentionally outside the schema contract.

## Current state to replace

The repository currently has the correct metadata but the wrong extraction mechanism:

- `ChessBattleEffects.cpp` owns `AuthorEnumDescriptor`, `PayloadFieldDescriptor`, `PayloadDescriptor`, `TimingDescriptor`, and the action, condition, macro, enum, payload, and probe registries.
- `tools/generate_chess_effect_schemas.py` reads `ChessBattleEffects.cpp` as text and reconstructs those objects with regular expressions and brace-aware initializer scanning.
- `tests/test_generate_chess_effect_schemas.py` invokes that Python generator for drift checking and separately uses `jsonschema` to exercise the emitted schemas.
- `.github/build-command.ps1` runs the Python schema test after a `kys_tests` build.

The descriptors are reliable. Parsing their C++ spelling is not: comments, formatting, helper functions, aliases, or a valid C++ refactor can break the Python extractor without changing the compiled facts. The new generator must consume the objects after C++ compilation instead of reverse-engineering their source representation.

## Source-of-truth boundary

### Descriptor-owned facts

The following data must continue to live in the C++ descriptor model and must be read directly by both the parser and schema renderer:

- author-facing action, condition, macro, timing, selector, attribute, status, and enum labels;
- payload field names;
- required versus optional fields;
- YAML node shapes;
- shared schema references such as effect number, selector, action list, condition list, and nested payload;
- enum descriptor references;
- dynamic key class and dynamic value shape;
- minimum-property and dynamic-alternative constraints;
- action and condition author forms;
- action, condition, and macro dispatch identity.

Compile-time completeness, uniqueness, and variant-coverage assertions remain beside these descriptor definitions. The schema renderer must not introduce another action-name, field-name, or enum-label table.

### Renderer-owned facts

Some facts are presentation or document-layout concerns rather than payload metadata. They may be ordinary C++ renderer code:

- JSON Schema Draft 2020-12 document structure;
- `$defs` names and `$ref` layout;
- the four formal config root layouts;
- schema `$id` and title values;
- schema-only combinations such as promoted-action XOR `動作`;
- schema-expressible timing rules such as `每幀` forbidding an interval and `每隔` requiring a positive interval;
- deterministic JSON formatting.

The four top-level layouts should remain explicit because they describe the surrounding config files, not an effect payload. Preserve their current key spelling and structure exactly; this task must not opportunistically migrate formal config roots.

If the renderer needs a label, field whitelist, payload shape, or enum list that already belongs to the authoring grammar, the descriptor model is incomplete and must be extended. Do not add a renderer-side parallel table.

## C++ module layout

### Extract descriptor visibility

Move the authoring descriptor types, registry data, and lookup access out of the anonymous namespace in `ChessBattleEffects.cpp` into an internal chess-core module. Suggested files:

- `src/ChessEffectAuthoringDescriptors.h`
- `src/ChessEffectAuthoringDescriptors.cpp`

The header should expose internal read-only views, preferably `std::span` or const references, rather than writable globals. It needs to expose the descriptor structures and accessors required by:

- the effect parser and its dispatch lookups;
- descriptor probe tests;
- the schema renderer.

The implementation file should own the constexpr arrays, pointer relationships, and existing compile-time assertions. Keeping the arrays in one implementation file avoids turning the entire registry into public inline header data while still allowing the parser and generator to consume the same compiled instances.

`ChessBattleEffects.cpp` then calls the shared accessors instead of directly referring to anonymous-namespace registries. This extraction is a linkage refactor only: descriptor contents and parser-normalized output must not change in this batch.

Do not expose these descriptors as a stable public game API. Place them in an internal namespace that makes their authoring/tooling purpose clear.

### Add the schema renderer

Add a host-tool renderer, suggested as:

- `tools/kys_effect_schema_codegen/ChessEffectSchemaRenderer.h`
- `tools/kys_effect_schema_codegen/ChessEffectSchemaRenderer.cpp`

The renderer reads the descriptor accessors and builds an in-memory JSON value for:

- effect number;
- selector;
- condition;
- action node;
- rule and rule list;
- all four formal config root schemas.

Use the existing Glaze dependency to serialize JSON. Use stable ordered object storage or an explicitly ordered construction strategy so the output is deterministic across runs. Output must be UTF-8 without BOM, use `\n` line endings, use one fixed indentation style, and end with one newline. Deterministic bytes make drift checks simple and make schema diffs reviewable.

The renderer should return a fixed collection of output filename and content pairs. File I/O and command-line handling belong to the executable layer, not to descriptor traversal.

### Add the host executable

Add a non-shipping native executable, suggested as:

- `tools/kys_effect_schema_codegen/main.cpp`
- target name `kys_effect_schema_codegen`

Its command-line contract should be deliberately small:

- `--output-dir <path>` generates all four schema files into that directory.
- Optional `--check-against <path>` compares the newly generated files with the four files in that directory and returns failure on missing or different files.

`--check-against` does not inspect YAML. It is only an artifact-drift comparison.

The executable should:

1. render all four documents in memory;
2. create the output directory;
3. write each result atomically through a temporary file and rename;
4. when checking, compare all four files and report every stale or missing path in one run;
5. print the exact update target to run when drift is found;
6. return a distinct nonzero status for generation failure versus schema drift if practical.

Do not give the executable a default source-tree output path. Requiring `--output-dir` prevents an ordinary invocation from silently rewriting checked-in files.

## Checked-in artifacts

Continue to check in:

- `schemas/chess_effects/chess_combos.schema.json`
- `schemas/chess_effects/chess_equipment.schema.json`
- `schemas/chess_effects/chess_magic_effects.schema.json`
- `schemas/chess_effects/chess_neigong.schema.json`

Checked-in schemas let VS Code provide completion before any local build and let non-native development environments consume the schema without a host toolchain.

There are two distinct workflows:

### Normal check workflow

1. Build the native code generator incrementally.
2. Generate all four files into a build intermediate directory.
3. Byte-compare that output with `schemas/chess_effects`.
4. Fail the relevant build if any file differs.
5. Leave the source tree unchanged.

Suggested intermediate locations are:

- MSBuild: the codegen project's `$(IntDir)chess_effect_schemas\` directory;
- CMake: `${CMAKE_CURRENT_BINARY_DIR}/generated/chess_effect_schemas/`.

### Explicit update workflow

`update_chess_effect_schemas` invokes the same compiled executable and explicitly uses `schemas/chess_effects` as its output directory. It replaces all four artifacts in one operation. This target is never a dependency of `Build`, `Rebuild`, tests, packaging, or the game.

The intended author loop after changing a descriptor is:

1. run `update_chess_effect_schemas`;
2. inspect the schema diff;
3. run a normal native build, which now passes the drift check;
4. start the game to validate actual config/runtime behavior when the descriptor change also affects content semantics.

## Visual Studio and MSBuild integration

### Projects

Add `kys_effect_schema_codegen` to `kys.sln` as a native x64 console project. It should:

- compile only the small main and renderer sources;
- reference `kys_chess_core` and obtain `kys_battle_core` through the existing dependency graph, or reference it explicitly if required by MSBuild linking;
- use the same C++ language version, UTF-8 compiler settings, runtime configuration, and vcpkg include paths as `kys_chess_core`;
- never be copied into game or CLI publish output.

Add a utility project or equivalent named `check_chess_effect_schemas`. Its normal `Build` target should:

- depend on `kys_effect_schema_codegen`;
- invoke it with the utility project's intermediate schema directory;
- pass `schemas/chess_effects` through `--check-against`;
- declare the checked-in schemas and generator binary as inputs for understandable MSBuild diagnostics, while still running the cheap comparison for every relevant build.

The same project should expose an explicit `UpdateChessEffectSchemas` target that depends on building the code generator but writes directly to `schemas/chess_effects`. It must not depend on the normal check target, because update is specifically needed when the check is stale.

### Dependency placement

Make these native final targets depend on `check_chess_effect_schemas`:

- `kys`;
- `kys_tests`;
- `kys_chess_cli`.

Do not make `kys_chess_core` depend on the check. The generator links `kys_chess_core`; adding the reverse dependency would create a cycle. A direct library-only build therefore compiles descriptors but does not run schema drift verification. All normal native developer entry points do run it, including the Debug game build and test build.

Ensure a solution build runs the check once through the project graph rather than once per downstream project. Directly building any one of the final projects must still reach the check.

Suggested developer commands are:

- normal verification: build `kys`, `kys_tests`, or `kys_chess_cli` as usual;
- explicit update: invoke the utility project's `UpdateChessEffectSchemas` target for the desired configuration and platform.

After this wiring exists, remove the Python schema-generation invocation from `.github/build-command.ps1`. Building its usual native targets already performs the C++ drift check. The script may continue to run Python schema-document tests if those tests are retained, but Python must no longer be required to derive or compare schemas from C++ source.

## CMake integration

Only define executable schema-generation targets for a native host build:

- require `NOT CMAKE_CROSSCOMPILING`;
- explicitly exclude Android and Emscripten as a readable safeguard.

For native builds:

1. add `kys_effect_schema_codegen` and link it to `kys_chess_core`;
2. add `check_chess_effect_schemas`, which always renders to `${CMAKE_CURRENT_BINARY_DIR}/generated/chess_effect_schemas/` and compares against the checked-in directory;
3. add `update_chess_effect_schemas`, excluded from the default build, which writes directly to the checked-in directory;
4. add the check as a dependency of each final target that exists: `kys`, `kys_tests`, and `kys_chess_cli`;
5. do not add it as a dependency of `kys_chess_core`.

The check target should run on every relevant build invocation. Compiling the generator and chess core remains incremental; only the inexpensive render-and-compare operation is repeated.

For Android and WebAssembly builds:

- do not create `kys_effect_schema_codegen`, `check_chess_effect_schemas`, or `update_chess_effect_schemas` as target-platform executables;
- do not add schema dependencies to game libraries or packaging;
- continue using the checked-in schemas for editor support;
- rely on a native developer or CI build to catch drift.

Do not solve cross-compilation by attempting to run `$<TARGET_FILE:kys_effect_schema_codegen>` from an Android or WebAssembly build. A future CI pipeline may build the host tool in a separate native build tree, but that is not required for this migration.

## Python cleanup and retained schema tests

Delete `tools/generate_chess_effect_schemas.py` after the C++ output matches the checked-in baseline and both native build systems are wired.

Refactor or rename `tests/test_generate_chess_effect_schemas.py` so it no longer:

- reads `ChessBattleEffects.cpp`;
- invokes the removed Python generator;
- owns descriptor-derived name or field lists.

It is reasonable to keep Python only as a consumer-side JSON Schema test because that tests the editor artifact itself. The retained tests may:

- call `Draft202012Validator.check_schema` for each generated document;
- validate the four formal YAML configs against their corresponding schemas;
- retain a small representative set of positive and negative structural examples, including timing shape and promoted-action XOR behavior.

These tests must not grow into a second runtime validator or reproduce every parser invariant. They validate that the emitted JSON Schema is internally valid and useful to an editor. The C++ drift target, not Python, proves that checked-in files match compiled descriptors.

## Implementation batches

### Batch 1: Descriptor extraction

1. Add the internal descriptor module and read-only accessors.
2. Move existing descriptor definitions and compile-time assertions without changing their contents.
3. Switch `ChessBattleEffects.cpp` and descriptor-driven tests to the shared accessors.
4. Build and run the existing C++ tests once as the checkpoint for this linkage-only refactor.

Do not modify the Python generator in this batch. It provides a temporary output baseline while descriptor linkage changes.

### Batch 2: C++ renderer and executable

1. Implement the common `$defs` renderer from compiled descriptors.
2. Implement the four explicit root layouts.
3. Add deterministic Glaze serialization and atomic output.
4. Add intermediate generation and byte-comparison behavior.
5. Generate into a temporary directory and compare all four files with the current checked-in schemas.

Any difference in this batch must be classified as either an intentional schema correction or a renderer defect. Do not silently accept a new baseline merely because the generator implementation changed.

### Batch 3: Native build wiring

1. Add the Visual Studio codegen and check projects and final-target dependencies.
2. Add native CMake codegen, check, and update targets.
3. Confirm ordinary Debug game and test builds leave `schemas/chess_effects` untouched.
4. Confirm stale checked-in output fails with actionable filenames and the update command.
5. Confirm the explicit update target refreshes all four files and a following normal build passes.

### Batch 4: Remove source parsing

1. Delete the Python C++-source generator.
2. Remove its drift test and build-script invocation.
3. Keep only consumer-side Python schema tests that still provide editor-artifact value.
4. Remove obsolete Python imports or dependencies if no remaining test uses them.
5. Update any developer documentation that still tells authors to run the Python generator.

### Batch 5: Cross-platform and editor confirmation

1. Confirm Android and WebAssembly configuration do not define or execute the host generator.
2. Confirm native CMake and Visual Studio builds both produce identical checked-in bytes.
3. Confirm VS Code associations still bind each formal YAML file to the correct checked-in schema.
4. Open representative effect rules in the YAML editor and confirm completion and structural diagnostics work without first starting the game.
5. Start the game once with the formal configs to preserve the existing runtime validation workflow.

## Focused verification matrix

Verification should be performed once per implementation batch, not after every small edit.

| Area | Required evidence |
| --- | --- |
| Descriptor extraction | Existing parser and descriptor tests pass with no normalized-rule changes |
| Renderer equivalence | Four C++-generated files match the accepted checked-in baseline byte-for-byte |
| Drift failure | Deliberately stale one intermediate comparison and confirm the native build names the stale schema |
| Source-tree safety | A normal Build and Rebuild leave checked-in schema timestamps and `git diff` unchanged |
| Explicit update | `update_chess_effect_schemas` refreshes all four artifacts and the next check passes |
| Visual Studio | Direct Debug builds of `kys` and `kys_tests` reach the check target |
| Native CMake | A final native target reaches `check_chess_effect_schemas` |
| Cross compilation | Android and WebAssembly configure without a runnable schema-codegen dependency |
| Schema usefulness | All four schemas are valid Draft 2020-12 documents and accept their formal YAML files |
| Runtime boundary | The game starts and loads the formal configs; no separate headless config executable was added |

## Completion criteria

This migration is complete only when:

- no script or test parses `ChessBattleEffects.cpp` to obtain metadata;
- parser dispatch and schema generation read the same compiled descriptor objects;
- checked-in schemas match the C++ renderer exactly;
- native final builds fail on schema drift without modifying the source tree;
- one explicit update target works in both Visual Studio/MSBuild and native CMake workflows;
- Android and WebAssembly builds never try to run a target-platform generator;
- editor schema associations continue to work before the game is built;
- starting the game remains the only authoritative end-to-end config/runtime validation path.

