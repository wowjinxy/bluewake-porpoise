# Offscreen GPU qualification

The frozen sparse-uniform candidate (`source2`) and conservative interface candidate (`attempt3`) passed actual Dawn D3D12 shader compilation, pipeline creation, binding, rendering and readback on 2026-10-08. The authored fixture uses synthetic rectangles and textures, with no game, window, surface or input. This is a correctness result, not a performance measurement or promotion decision.

The same case source was compiled independently against the active baseline, sparse-uniform header/generators, and interface header/generators. Each executable initializes named constant members in its own ABI; reordered raw constant bytes never cross between them. All 89 cases passed with exact RGBA8 and Depth32Float byte equality and zero Dawn validation errors:

| Executable | Variants | Submitted frames | Internal checks |
| --- | ---: | ---: | ---: |
| Active baseline | 3 | 267 | 2,930,364 |
| Sparse uniforms | 6 | 534 | 11,695,008 |
| Interfaces off/on | 10 | 890 | 23,382,537 |

The 1,691 recorded images include 1,424 candidate-to-baseline comparisons; the remaining 267 entries are baseline self references. Independently compiled interface-pruning-off shaders matched the uniform executable byte for byte in all 768 saved draw shader-text pairs. Interface-pruning-on rendered with complete/sparse uniform bindings and full/selective vertex layouts. Canonical uber variants compared against matching baseline uber output, without assuming uber equals specialized behavior for unsupported features.

Cases include the existing 43 selective-layout cases, 32 independent primary-light oracles covering all eight light slots and four attenuation functions (each draws passthrough and TEV paths), and 14 additional material, normal-matrix-bank, Color0/Color1 texgen, near-end indexed matrix, ordered/forward emboss, independent indirect sample, coordinate fallback and mixed raster-channel cases. Earlier cases also cover blending/destination alpha, HUD/fog, alpha discard, scissor, reverse-Z and late depth output. Static input layouts come from actual generated shader declarations; prefix lengths come from the production uniform-use helper.

The corrected fixture uses an explicit complete three-bank layout and nonzero dynamic offsets, matching the production contract. It binds bank ranges of 656/1536/640 bytes, copies only the helper-selected prefixes, and poisons all remaining bytes and guard regions. Zero-prefix banks remain bound with poison. Complete and pixel constant blocks also use nonzero dynamic offsets. This checks that shader-visible results do not depend on unstaged tails; it does not execute production staging or deduplication.

The first attempt was retained as a fixture failure. Its baseline passed, and its uniform run passed all primary-light cases before a register-only material shader hit an auto-layout error: Dawn omitted an unused light binding while the conservative helper still requested it. Production uses an explicit three-binding layout, where that unused entry is legal. The repair changed only the authored fixture layout/bindings. All six already-compiled generator objects were reused only after checking their hashes, original actual `-MD` closures, current actual `-M` dependency-content equality, and compiler flags. Three changed fixture translation units were recompiled. All three final linker reproduction archives matched their ten declared actual inputs. The original failed fixture, runner, preparer, preparation receipt and cases are retained privately with their original receipt hashes.

Private evidence references (not bundled binaries or captures):

| Evidence | SHA-256 |
| --- | --- |
| `build/k7-uniforms-20261008/source2/source-receipt.json` | `64e39f9cdd446858952190cc5076e537a81632d738e8de0d0075b234ed3c246a` |
| `build/k7-interfaces-20261008/attempt3/source-receipt.json` | `f446b61fe5c22feb964229d474c1f2590015dccd3d8d6c3b34b06ef9d76853b3` |
| `build/k7-uniforms-20261008/gpu1/attempt1/result.json` (retained fixture failure) | `f4990a9cd2c333e665a73afe8779d5836ab97ecf8ebe5bd23eff607c53abda68` |
| `build/k7-uniforms-20261008/gpu1/attempt2/result.json` (GPU pass) | `2bac3a8b55ee8e548bae08f6b69751ab78f4a040bd53a57780d943297b91a351` |
| Final `tests/uniform_pixels_test.cpp` | `975efdc9d5a4b1ab3ca186ce0a454e2aca5ec120f8bef6370b3ddab97c1dddd5` |
| Final `tools/run_gpu_fixture.py` | `734744d5967ddbef77f697bda976658abd717819764a6372064e5df242211347` |
| `tools/gpu-provenance/prepare_fixture.py` | `9df54ffa8c503d2c976b7905e32b1f2c602974bd782af7766cfa84ba2cc8fe43` |
| `tools/gpu-provenance/cases.cpp.inc` | `3ca0f4e89ad5c2f95ca02cd541a08948542202a8af6a9549247931aa5020798a` |

Actual dependency counts for fixture/specialized/uber translation units were 277/211/202 for baseline and 277/214/202 for both candidates. The runner/preparer are preserved exact private command recipes; their original paths and private toolchain/Dawn/environment prerequisites are not portable assumptions. Construction records describe the earlier copied scaffold chronologically; the final qualified source and runner hashes above supersede those construction file hashes.

Limits: every texture slot shares the same authored image, so this does not test distinct movie planes. The standalone fixture does not execute the production pipeline factory, early-depth multipass, asynchronous fallback transition, staging failure rollback, batching, interpolation, Smooth Motion, presentation, or gameplay. Uber late-Z/early-depth feature equivalence is outside this result. Malformed zero-texgen textured/indirect keys remain pre-existing invalid generator inputs. Focused interpolation tests do not resolve the retained broad interpolation ASan baseline failure. Shader/interface pruning may duplicate driver dead-code elimination; no GPU timing, frame-rate gain, or native speedup follows from these checks.
