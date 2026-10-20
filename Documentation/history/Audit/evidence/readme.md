# Audit Execution Evidence

Snapshot: 15 October 2026. Logs are complete command outputs. Nonzero results include expected defect reproductions and explicitly documented environment/setup failures; they are not all project build failures.

[Environment, repository integrity and fetched dependency commits](snapshot.json). [Original API lexical candidates](original_api_candidates.json).

| Run | Exit | Seconds | Exact command and context | Output |
| --- | ---: | ---: | --- | --- |
| abc_cycle | 124 | 3.002 | [Metadata](abc_cycle.json) | [Log](abc_cycle.log) |
| abc_cycle_build | 0 | 2.142 | [Metadata](abc_cycle_build.json) | [Log](abc_cycle_build.log) |
| aggregate_dispatch | 1 | 0.002 | [Metadata](aggregate_dispatch.json) | [Log](aggregate_dispatch.log) |
| aggregate_probe_build | 1 | 0.478 | [Metadata](aggregate_probe_build.json) | [Log](aggregate_probe_build.log) |
| aggregate_probe_build_corrected | 0 | 0.678 | [Metadata](aggregate_probe_build_corrected.json) | [Log](aggregate_probe_build_corrected.log) |
| engine_contract_remove | 1 | 0.007 | [Metadata](engine_contract_remove.json) | [Log](engine_contract_remove.log) |
| engine_contract_values | 0 | 0.013 | [Metadata](engine_contract_values.json) | [Log](engine_contract_values.log) |
| engine_probe_build | 1 | 33.292 | [Metadata](engine_probe_build.json) | [Log](engine_probe_build.log) |
| engine_probe_build_clang | 1 | 8.987 | [Metadata](engine_probe_build_clang.json) | [Log](engine_probe_build_clang.log) |
| engine_probe_build_clang17 | 0 | 26.988 | [Metadata](engine_probe_build_clang17.json) | [Log](engine_probe_build_clang17.log) |
| engine_remove_symbolization | 0 | 0.172 | [Metadata](engine_remove_symbolization.json) | [Log](engine_remove_symbolization.log) |
| lithtech_configure | 1 | 0.009 | [Metadata](lithtech_configure.json) | [Log](lithtech_configure.log) |
| lithtech_policy_configure | 1 | 0.343 | [Metadata](lithtech_policy_configure.json) | [Log](lithtech_policy_configure.log) |
| nomad_main_build | 1 | 0.134 | [Metadata](nomad_main_build.json) | [Log](nomad_main_build.log) |
| nomad_main_build_corrected | 0 | 4.524 | [Metadata](nomad_main_build_corrected.json) | [Log](nomad_main_build_corrected.log) |
| nomad_tests_build | 0 | 6.26 | [Metadata](nomad_tests_build.json) | [Log](nomad_tests_build.log) |
| nomad_tests_default_runtime | 150 | 0.003 | [Metadata](nomad_tests_default_runtime.json) | [Log](nomad_tests_default_runtime.log) |
| nomad_tests_rollforward | 1 | 0.205 | [Metadata](nomad_tests_rollforward.json) | [Log](nomad_tests_rollforward.log) |
| nomad_tests_synthetic_root | 0 | 0.341 | [Metadata](nomad_tests_synthetic_root.json) | [Log](nomad_tests_synthetic_root.log) |
| nomadengine_build_override | 1 | 124.042 | [Metadata](nomadengine_build_override.json) | [Log](nomadengine_build_override.log) |
| nomadengine_build_vr | 0 | 52.206 | [Metadata](nomadengine_build_vr.json) | [Log](nomadengine_build_vr.log) |
| nomadengine_configure | 1 | 110.917 | [Metadata](nomadengine_configure.json) | [Log](nomadengine_configure.log) |
| nomadengine_configure_override | 0 | 3.568 | [Metadata](nomadengine_configure_override.json) | [Log](nomadengine_configure_override.log) |
| nomadengine_configure_vr | 0 | 3.467 | [Metadata](nomadengine_configure_vr.json) | [Log](nomadengine_configure_vr.log) |
| nomadengine_ctest_inventory | 0 | 0.024 | [Metadata](nomadengine_ctest_inventory.json) | [Log](nomadengine_ctest_inventory.log) |
| openblood2_build_override | 0 | 109.605 | [Metadata](openblood2_build_override.json) | [Log](openblood2_build_override.log) |
| openblood2_configure | 1 | 101.066 | [Metadata](openblood2_configure.json) | [Log](openblood2_configure.log) |
| openblood2_configure_override | 0 | 2.588 | [Metadata](openblood2_configure_override.json) | [Log](openblood2_configure_override.log) |
| openblood2_ctest_inventory | 0 | 0.025 | [Metadata](openblood2_ctest_inventory.json) | [Log](openblood2_ctest_inventory.log) |
| openblood2_help | 0 | 0.003 | [Metadata](openblood2_help.json) | [Log](openblood2_help.log) |
