# Chess Effect Schema C++ Generation and Build Integration Plan

Status: implemented (2026-08-24). The compiled descriptor renderer, native build integration, generated-file Git policy, and consumer-side schema tests are complete.

## Purpose

Replace `tools/generate_chess_effect_schemas.py` as the source of generated chess-effect schemas. The replacement reads the real compiled C++ authoring descriptors and produces four local generated JSON Schema files used by the YAML editor.

This plan implements the schema portion of [效果規則通用簡式與舊語法移除設計](效果規則通用簡式與舊語法移除設計.md). It does not change the authoring syntax, parser semantics, or runtime validation contract. Both documents use the implemented policy: schemas are ignored build output, while `.vscode` remains ignored and untracked.

The required end state is:

- Compiled C++ descriptors are the only source of truth for effect authoring names, fields, shapes, requiredness, nested payloads, enum labels, and dynamic-key classes.
- A small host-only C++ executable renders JSON Schema directly from those descriptor objects.
- Every relevant native developer build regenerates the schemas in a stable repository-local generated directory used by the editor.
- The generated schema files are removed from version control and ignored after migration.
- Schema generation is an automatic dependency of relevant native final builds; there is no separate author-facing generation or update workflow.
- Android and WebAssembly builds do not try to execute a target-platform generator.
- No code reads or parses C++ source text to discover schema metadata.
- The existing `.vscode` ignore policy remains unchanged; do not add a `!.vscode` exception to `.gitignore`.

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

Use the existing Glaze dependency to serialize JSON. Use stable ordered object storage or an explicitly ordered construction strategy so the output is deterministic across runs. Output must be UTF-8 without BOM, use `\n` line endings, use one fixed indentation style, and end with one newline. Deterministic output avoids unnecessary editor file-system churn and permits exact renderer-equivalence tests during the migration.

The renderer should return a fixed collection of output filename and content pairs. File I/O and command-line handling belong to the executable layer, not to descriptor traversal.

### Add the host executable

Add a non-shipping native executable, suggested as:

- `tools/kys_effect_schema_codegen/main.cpp`
- target name `kys_effect_schema_codegen`

Its command-line contract should be deliberately small:

- `--output-dir <path>` generates all four schema files into that directory.

The executable should:

1. render all four documents in memory;
2. create the output directory;
3. write each result atomically through a temporary file and rename;
4. avoid replacing a destination whose bytes are already identical, preserving its timestamp;
5. report every generation or write failure with its destination path;
6. return nonzero if any of the four files cannot be produced.

Do not give the executable an implicit output path. Build integration passes the stable editor-schema directory explicitly, while tests may pass an isolated temporary directory.

## Generated artifacts and Git policy

Generate these four local artifacts:

- `schemas/chess_effects/chess_combos.schema.json`
- `schemas/chess_effects/chess_equipment.schema.json`
- `schemas/chess_effects/chess_magic_effects.schema.json`
- `schemas/chess_effects/chess_neigong.schema.json`

These paths remain stable so the existing local `yaml.schemas` associations continue to work, but the files are build products rather than repository sources.

At the final migration step:

1. remove the four schema files from the Git index;
2. add precise ignore entries for the four generated files, or for `schemas/chess_effects/` if that directory will contain generated artifacts only;
3. do not commit regenerated schema output;
4. do not add `.vscode` files to Git;
5. retain the existing `.vscode` entry in `.gitignore` and do not add a `!.vscode` negation exception.

The repository therefore does not promise editor schemas immediately after a fresh checkout. A relevant native build creates them locally. This trade-off is intentional: compiled C++ descriptors remain the source, and generated editor artifacts do not require review or synchronization in Git.

### Normal generation workflow

1. Build the native code generator incrementally.
2. Generate all four files directly into `schemas/chess_effects/`.
3. Leave identical files untouched to avoid editor reload churn.
4. Fail the relevant build only if generation or writing fails.

The intended author loop after changing a descriptor is:

1. build `kys`, `kys_tests`, or `kys_chess_cli` normally;
2. allow the generated editor schemas to refresh locally as part of that build;
3. use the YAML editor for completion and structural feedback;
4. start the game to validate actual config/runtime behavior when the descriptor change also affects content semantics.

## Visual Studio and MSBuild integration

### Projects

Add `kys_effect_schema_codegen` to `kys.sln` as a native x64 console project. It should:

- compile only the small main and renderer sources;
- reference `kys_chess_core` and obtain `kys_battle_core` through the existing dependency graph, or reference it explicitly if required by MSBuild linking;
- use the same C++ language version, UTF-8 compiler settings, runtime configuration, and vcpkg include paths as `kys_chess_core`;
- never be copied into game or CLI publish output.

Add a `GenerateChessEffectSchemas` step to the codegen project's normal `Build` target. It should:

- run after the executable is available;
- invoke it with `$(SolutionDir)schemas\chess_effects` as the explicit output directory;
- run whenever the codegen project is reached through a relevant final build, even if compilation itself was already up to date;
- fail the enclosing build if the generator cannot produce all four artifacts.

Do not add a separate utility project, public generation target, or update target. The codegen project's automatic build step is the only build-system entry point for schema generation.

### Dependency placement

Make these native final targets depend on the `kys_effect_schema_codegen` project for build ordering, without treating its executable as a link input:

- `kys`;
- `kys_tests`;
- `kys_chess_cli`.

Do not make `kys_chess_core` depend on generation. The generator links `kys_chess_core`; adding the reverse dependency would create a cycle. A direct library-only build therefore compiles descriptors but does not refresh editor schemas. All normal native developer entry points do refresh them, including the Debug game build and test build.

Ensure a solution build reaches the codegen project once through the project graph rather than invoking the executable independently from every downstream project. Directly building any one of the final projects must still reach generation. There is no separately documented schema command; authors build a normal final target.

After this wiring exists, remove the Python schema-generation invocation from `.github/build-command.ps1`. Building its usual native targets already runs the C++ generator; when `kys_tests` is included, the script then runs the retained consumer-side schema tests. Python must no longer be required to derive schemas from C++ source.

## CMake integration

Only define executable schema-generation targets for a native host build:

- require `NOT CMAKE_CROSSCOMPILING`;
- explicitly exclude Android and Emscripten as a readable safeguard.

For native builds:

1. add `kys_effect_schema_codegen` and link it to `kys_chess_core`;
2. add one `add_custom_command(OUTPUT ...)` that renders the four files directly to `${KYS_ROOT}/schemas/chess_effects/` and depends on `kys_effect_schema_codegen`;
3. give those outputs one private internal custom-target owner, then make each final target that exists depend on that owner: `kys`, `kys_tests`, and `kys_chess_cli`;
4. let CMake's single output owner run the command once when files are missing or the compiled generator has changed, including parallel full builds;
5. do not add the generated outputs to `kys_chess_core`.

Do not add a standalone author-facing `generate_chess_effect_schemas` or `update_chess_effect_schemas` target. The private output-owner target is only an internal build-graph serialization point. Compiling the generator and chess core remains incremental, and deleting a local schema causes the next relevant native build to recreate all required output.

For Android and WebAssembly builds:

- do not create `kys_effect_schema_codegen` or schema-output custom commands for the target platform;
- do not add schema dependencies to game libraries or packaging;
- use schemas left by a native build for local editor support, if present;
- accept that a cross-platform-only checkout has no generated editor schemas until a native host generation step is run.

Do not solve cross-compilation by attempting to run `$<TARGET_FILE:kys_effect_schema_codegen>` from an Android or WebAssembly build. A future CI pipeline may build the host tool in a separate native build tree, but that is not required for this migration.

## Python cleanup and retained schema tests

Delete `tools/generate_chess_effect_schemas.py` after the C++ output matches the pre-migration schema baseline and both native build systems are wired.

Refactor or rename `tests/test_generate_chess_effect_schemas.py` so it no longer:

- reads `ChessBattleEffects.cpp`;
- invokes the removed Python generator;
- owns descriptor-derived name or field lists.

It is reasonable to keep Python only as a consumer-side JSON Schema test because that tests the editor artifact itself. The retained tests may:

- call `Draft202012Validator.check_schema` for each generated document;
- validate the four formal YAML configs against their corresponding schemas;
- retain a small representative set of positive and negative structural examples, including timing shape and promoted-action XOR behavior.

These tests must not grow into a second runtime validator or reproduce every parser invariant. They validate that the emitted JSON Schema is internally valid and useful to an editor. They run after C++ generation and never establish a second descriptor source of truth.

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
4. Generate into a temporary directory and compare all four files with the current pre-migration tracked schemas.
5. Add a renderer test that generates twice and proves the second write leaves identical files unchanged.

Any difference in this batch must be classified as either an intentional schema correction or a renderer defect. Do not silently accept a new baseline merely because the generator implementation changed.

### Batch 3: Native build wiring

1. Add the Visual Studio codegen project, its automatic generation step, and final-project dependencies.
2. Add the native CMake codegen executable and generated-output dependencies without a standalone generation target.
3. Confirm ordinary Debug game and test builds produce all four files under `schemas/chess_effects`.
4. Delete one local generated file and confirm the next relevant build recreates it.
5. Confirm a generation failure fails the enclosing native build with the destination path.

### Batch 4: Remove source parsing

1. Delete the Python C++-source generator.
2. Remove its drift test and build-script invocation.
3. Keep only consumer-side Python schema tests that still provide editor-artifact value.
4. Remove obsolete Python imports or dependencies if no remaining test uses them.
5. Remove the four generated schemas from the Git index and add precise schema-output ignore rules.
6. Leave `.vscode` ignored and untracked; do not add a `!.vscode` negation rule.
7. Update any developer documentation that still tells authors to run the Python generator or commit generated schemas.

### Batch 5: Cross-platform and editor confirmation

1. Confirm Android and WebAssembly configuration do not define or execute the host generator.
2. Confirm native CMake and Visual Studio builds both produce identical local bytes.
3. Confirm the existing local VS Code associations bind each formal YAML file to its generated schema after a native build.
4. Open representative effect rules in the YAML editor and confirm completion and structural diagnostics work without starting the game.
5. Start the game once with the formal configs to preserve the existing runtime validation workflow.

## Focused verification matrix

Verification should be performed once per implementation batch, not after every small edit.

| Area | Required evidence |
| --- | --- |
| Descriptor extraction | Existing parser and descriptor tests pass with no normalized-rule changes |
| Renderer equivalence | Before removing the old files from Git, four C++-generated files match the accepted baseline byte-for-byte |
| Regeneration | Removing one generated file causes the next relevant native build to recreate it |
| Git policy | No schema output or `.vscode` file is tracked; a normal build adds no unignored change |
| Visual Studio | Direct Debug builds of `kys` and `kys_tests` produce the four local schemas |
| Native CMake | A final native target reaches the schema-output dependency and produces the same bytes |
| Cross compilation | Android and WebAssembly configure without a runnable schema-codegen dependency |
| Schema usefulness | All four schemas are valid Draft 2020-12 documents and accept their formal YAML files |
| Runtime boundary | The game starts and loads the formal configs; no separate headless config executable was added |

## Completion criteria

This migration is complete only when:

- no script or test parses `ChessBattleEffects.cpp` to obtain metadata;
- parser dispatch and schema generation read the same compiled descriptor objects;
- the four generated schema files are absent from the Git index and covered by precise ignore rules;
- `.vscode` remains ignored and untracked, with no `!.vscode` negation rule;
- relevant native final builds create or refresh the four local schemas automatically;
- there is no standalone author-facing generation, check, or update target;
- Android and WebAssembly builds never try to run a target-platform generator;
- existing local editor schema associations work after a relevant native build;
- starting the game remains the only authoritative end-to-end config/runtime validation path.
