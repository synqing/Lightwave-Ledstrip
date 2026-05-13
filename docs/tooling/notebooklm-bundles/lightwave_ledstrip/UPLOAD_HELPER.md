---
abstract: "Ordered upload checklist for Rev 4 Lightwave-Ledstrip NotebookLM bundle (163 sources). Upload order: source-code bundles first (.txt) to anchor the architecture, then root authority docs (CLAUDE/AGENTS/BACKLOG), then forensic audit trail, then architecture/reference, then technical docs, then research summaries, then governance. Includes notebook ID and re-upload protocol."
---

# UPLOAD HELPER -- Lightwave-Ledstrip NotebookLM Rev 4

**Notebook ID:** `92d45c0b-83c7-4971-aa9a-2c9ee13b06d4`
**Bundle path:** `docs/tooling/notebooklm-bundles/lightwave_ledstrip/`
**Source count:** 163 (158 .md + 5 .txt) -- within 300-source NotebookLM ceiling
**Generated:** 2026-05-13

## Re-upload protocol

1. Delete the existing 127 sources from notebook `92d45c0b-83c7-4971-aa9a-2c9ee13b06d4` (Rev 3 bundle).
2. Upload the 163 sources below in the listed order. The .txt source-code bundles go first so the architecture anchors are present before referenced docs.
3. After upload completes, run the Phase 2 `chat_configure` call from `CC_CLI_NOTEBOOKLM_INTEGRATION_PROMPT.md`.
4. Run the 5 Phase 3 test queries and produce the Phase 4 verification report.
5. Update `docs/tooling/notebooklm-bundles/NOTEBOOK_REGISTRY.md` -- bump source count to 163 and last-synced to 2026-05-13.

## Files to upload (in order)

### Group 1: Source code bundles (5)

- `_BUNDLE_firmware_contracts.txt`
- `_BUNDLE_firmware_actors.txt`
- `_BUNDLE_protocol_contracts.txt`
- `_BUNDLE_ios_architecture.txt`
- `_BUNDLE_tab5_architecture.txt`

### Group 2: Root authority (7)

- `CLAUDE.md`
- `AGENTS.md`
- `BACKLOG.md`
- `CHANGELOG.md`
- `README.md`
- `CONTRIBUTING.md`
- `TRADEMARK.md`

### Group 3: Forensic WiFi audit trail (6 -- preserved from Rev 3)

- `_FORENSIC_WIFI_REPORT.md`
- `_FORENSIC_WIFI_GENESIS.md`
- `_FORENSIC_WIFI_A_GIT.md`
- `_FORENSIC_WIFI_B_SOURCE.md`
- `_FORENSIC_WIFI_C_MEMORY.md`
- `_FORENSIC_WIFI_D_DOCTRINE.md`

### Group 4: All remaining sources, alphabetical (145)

- `docs_adr_zone-composer-architecture-decisions.md`
- `docs_CAPTURE_PIPELINE_REFERENCE.md`
- `docs_CAPTURE_TEST_SUITES.md`
- `docs_cron_k1-launch-research.md`
- `docs_DEPENDENCY_LICENSES.md`
- `docs_design_VOICE_CONTROL_EXPLORATION_PLAN.md`
- `docs_HUNT-WAVE1-spec-changes.md`
- `docs_K1_ECOSYSTEM_API_ROADMAP.md`
- `docs_K1_Waveform_Algorithm_Breakdown.md`
- `docs_marketing_K1-DUAL-STATE-POSITIONING.md`
- `docs_marketing_K1-LANDING-PAGE-BUILD-SPEC.md`
- `docs_marketing_K1-LAUNCH-VIDEO-SPEC.md`
- `docs_marketing_K1-STRATEGY.md`
- `docs_marketing_K1-TAGLINES.md`
- `docs_MULTIPLIER_STACK.md`
- `docs_ONSET_CAPTURE_WORKFLOW.md`
- `docs_protocol_README.md`
- `docs_protocol_zones-command-matrix.md`
- `docs_protocol_zones-serial-json-parity.md`
- `docs_superpowers_ios-firmware-parity-phase-1.md`
- `docs_superpowers_ios-firmware-parity-phase-2.md`
- `docs_superpowers_ios-firmware-parity-phase-3-scoping.md`
- `docs_temporary_projects_synqmatrix-naming-review_00-INDEX.md`
- `docs_temporary_projects_synqmatrix-naming-review_01-audio-contracts.md`
- `docs_temporary_projects_synqmatrix-naming-review_02-audio-backends-and-actor.md`
- `docs_temporary_projects_synqmatrix-naming-review_03-audio-pipeline-onset-tempo.md`
- `docs_temporary_projects_synqmatrix-naming-review_04-renderer-actor.md`
- `docs_temporary_projects_synqmatrix-naming-review_05-actors-and-base.md`
- `docs_temporary_projects_synqmatrix-naming-review_06-synqmatrix-and-plugin-api.md`
- `docs_temporary_projects_synqmatrix-naming-review_07-network-server-routes.md`
- `docs_temporary_projects_synqmatrix-naming-review_08-network-handlers-and-ws-commands.md`
- `docs_temporary_projects_synqmatrix-naming-review_09-serial-and-capture.md`
- `docs_temporary_projects_synqmatrix-naming-review_10A-hal-config.md`
- `docs_temporary_projects_synqmatrix-naming-review_10B-core-utilities.md`
- `docs_TOOLCHAIN_IMPLEMENTATION_GUIDE.md`
- `docs_tooling_claude-mem-usage-optimisation-2026-05-02.md`
- `docs_WORKFLOW_ROUTING.md`
- `firmware-v3_CONSTRAINTS.md`
- `firmware-v3_docs_api_api-legacy.md`
- `firmware-v3_docs_api_api-v1.md`
- `firmware-v3_docs_api_api-v2.md`
- `firmware-v3_docs_architecture_DEFENSIVE_BOUNDS_CHECKING.md`
- `firmware-v3_docs_architecture_WEB_SERVER_MODULAR_ARCHITECTURE.md`
- `firmware-v3_docs_AUDIO_REACTIVE_EFFECTS_PACK_152_161_DECOMPOSITION.md`
- `firmware-v3_docs_audio-visual_ADR_2026-03-25_FIRST_CLASS_ONSET_SURFACE.md`
- `firmware-v3_docs_audio-visual_AUDIO_OUTPUT_SPECIFICATIONS.md`
- `firmware-v3_docs_audio-visual_AUDIO_SYSTEM_ARCHITECTURE_VISUAL.md`
- `firmware-v3_docs_audio-visual_AUDIO_SYSTEM_ARCHITECTURE.md`
- `firmware-v3_docs_audio-visual_audio-visual-contract-surface.md`
- `firmware-v3_docs_audio-visual_audio-visual-semantic-mapping.md`
- `firmware-v3_docs_audio-visual_bins64-adaptive-guidance.md`
- `firmware-v3_docs_audio-visual_COLOR_PALETTE_SYSTEM.md`
- `firmware-v3_docs_audio-visual_IMPLEMENTATION_PATTERNS.md`
- `firmware-v3_docs_audio-visual_MUSICAL_LOGIC_CANONICAL_MODEL.md`
- `firmware-v3_docs_audio-visual_README.md`
- `firmware-v3_docs_audio-visual_TROUBLESHOOTING.md`
- `firmware-v3_docs_audio-visual_VISUAL_PIPELINE_MECHANICS.md`
- `firmware-v3_docs_audit_VP_RENDER_PATH_LAYER_AUDIT_2026-05-05.md`
- `firmware-v3_docs_audit_VP_VALIDATION_PROTOCOL_2026-05-06.md`
- `firmware-v3_docs_CONTEXT_GUIDE.md`
- `firmware-v3_docs_CQRS_STATE_ARCHITECTURE.md`
- `firmware-v3_docs_debugging_DEBUG_SYSTEM.md`
- `firmware-v3_docs_debugging_MABUTRACE_GUIDE.md`
- `firmware-v3_docs_debugging_TRACE_INSTRUMENTATION_SPEC.md`
- `firmware-v3_docs_debugging_VP_STACK_INTROSPECTION_COMMAND_SPEC.md`
- `firmware-v3_docs_design_INFERENCE_TASK_DECISION_BRIEF.md`
- `firmware-v3_docs_design_INFERENCE_TASK_PLACEMENT_MATRIX.md`
- `firmware-v3_docs_design_ONSET_DETECTOR_SPEC.md`
- `firmware-v3_docs_EFFECT_AUTHORING_STANDARD_V2.md`
- `firmware-v3_docs_EFFECT_DEVELOPMENT_STANDARD.md`
- `firmware-v3_docs_EFFECT_FRAMEWORK_STANDARD.md`
- `firmware-v3_docs_EFFECTS_BEHAVIORAL_REFERENCE.md`
- `firmware-v3_docs_effects-catalog_EFFECTS_INVENTORY.md`
- `firmware-v3_docs_effects-catalog_MATH_APPENDIX.md`
- `firmware-v3_docs_effects-catalog_PATTERN_TAXONOMY.md`
- `firmware-v3_docs_GOOD_LIGHT_SHOW_TAXONOMY.md`
- `firmware-v3_docs_gradient-system.md`
- `firmware-v3_docs_measurement_protocols_m1_lgp_fringe.md`
- `firmware-v3_docs_migration_WEB_SERVER_REFACTOR_MIGRATION_GUIDE.md`
- `firmware-v3_docs_MusicAware_Audit_And_Gap_Analysis.md`
- `firmware-v3_docs_NON_AUDIO_EFFECTS_PACK_132_151_AUDIO_REFACTOR_BLUEPRINT.md`
- `firmware-v3_docs_OTA_UPDATE_GUIDE.md`
- `firmware-v3_docs_p1-09-migration-cookbook.md`
- `firmware-v3_docs_p1-09-zone-state-followup-plan.md`
- `firmware-v3_docs_performance_RMT_SHOW_PATH_2026-02-28.md`
- `firmware-v3_docs_performance_VALIDATION_OVERHEAD.md`
- `firmware-v3_docs_reference_audio-pipeline-parameters.md`
- `firmware-v3_docs_reference_codebase-map.md`
- `firmware-v3_docs_reference_emotiscope-algorithms.md`
- `firmware-v3_docs_reference_fsm-reference.md`
- `firmware-v3_docs_reference_k1-vs-wled-audio-comparison.md`
- `firmware-v3_docs_reference_README-audio-research.md`
- `firmware-v3_docs_reference_wled-audio-reactive-analysis.md`
- `firmware-v3_docs_reference_wled-frequency-mapping-visual.md`
- `firmware-v3_docs_reference_wled-parameter-extract.md`
- `firmware-v3_docs_research_c1_mic_domain_envelope_audit_2026-05-06.md`
- `firmware-v3_docs_research_c2_feature_effect_dwell_matrix_2026-05-06.md`
- `firmware-v3_docs_research_f6_controlbus_num_zones_audit_2026-05-06.md`
- `firmware-v3_docs_research_k1_medium_phase0_captain_review_table_2026-05-12.md`
- `firmware-v3_docs_research_k1_medium_phase0_decision_record_2026-05-12.md`
- `firmware-v3_docs_research_k1_songaware_control_surface_schema_2026-05-12.md`
- `firmware-v3_docs_research_k1_songaware_decision_record_2026-05-12.md`
- `firmware-v3_docs_research_k1_songaware_director_mode_matrix_2026-05-12_analysis.md`
- `firmware-v3_docs_research_k1_songaware_feasibility_2026-05-12_analysis.md`
- `firmware-v3_docs_research_k1_visual_characterisation_database.md`
- `firmware-v3_docs_research_k1v2_sram_psram_reclaim_handoff_2026-05-06.md`
- `firmware-v3_docs_research_lgp_beat_emotiscope_architecture_review_2026-05-06.md`
- `firmware-v3_docs_research_phase1b_audioctx_copy_reduction_report.md`
- `firmware-v3_docs_research_phase1b_controlbus_dram_relocation_README.md`
- `firmware-v3_docs_research_phase1b_controlbus_dram_report.md`
- `firmware-v3_docs_research_phase1b_controlbus_handoff_tier2_report.md`
- `firmware-v3_docs_research_trinity_inactive_status_note_2026-05-06.md`
- `firmware-v3_docs_STIMULUS_CONTROL_CONTRACT.md`
- `firmware-v3_docs_testing_AUDIO_TEST_HARNESS.md`
- `firmware-v3_docs_testing_METRICS_REFERENCE.md`
- `firmware-v3_docs_testing_TEST_SCENARIOS.md`
- `instructions_changelog__fragment-template.md`
- `instructions_changelog_2026-04-27--firmware-v3--afs-v2-contract-docs.md`
- `instructions_changelog_2026-04-27--firmware-v3--afs-v2-effect-api-lock.md`
- `instructions_changelog_2026-04-27--firmware-v3--afs-v2-phase-1b-instrumentation.md`
- `instructions_changelog_2026-04-27--firmware-v3--mabutrace-capture-tooling.md`
- `instructions_changelog_2026-04-27--firmware-v3--phase-5-effect-exemplars.md`
- `instructions_changelog_2026-04-27--firmware-v3--phase-5-native-harness.md`
- `instructions_GOV-amendment-001-dual-pipeline.md`
- `instructions_GOV-research-pipeline.md`
- `instructions_naming-policy-v1.md`
- `instructions_repo-governance-v1.md`
- `instructions_work-blocking-protocol-v1.md`
- `lightwave-ios-v2_docs_CLAUDE.md`
- `lightwave-ios-v2_docs_DESIGN_SPEC.md`
- `lightwave-ios-v2_docs_reference_codebase-map.md`
- `lightwave-ios-v2_docs_reference_fsm-reference.md`
- `tab5-encoder_docs_ARCHITECTURE_DECISION_PRINCIPLES.md`
- `tab5-encoder_docs_CONTROLSURFACE_RESEARCH.md`
- `tab5-encoder_docs_DESIGN_BRIEF.md`
- `tab5-encoder_docs_EFFECT_ORDER_REFERENCE.md`
- `tab5-encoder_docs_IMPLEMENTATION_SPEC.md`
- `tab5-encoder_docs_MENU_SYSTEM_RESEARCH.md`
- `tab5-encoder_docs_PRODUCT_DECISION_PRINCIPLES.md`
- `tab5-encoder_docs_reference_codebase-map.md`
- `tab5-encoder_docs_reference_fsm-reference.md`
- `tab5-encoder_docs_reference_lvgl-component-reference.md`
- `tab5-encoder_docs_ROW2_EFFECT_PARAMETER_SPEC.md`
- `tab5-encoder_docs_STIMULUS_CONTROL_CONTRACT.md`
- `tab5-encoder_docs_UI_DESIGN_RESEARCH.md`
- `tab5-encoder_docs_ZONE_COMPOSER_V2_SPEC.md`

---
**Document Changelog**

| Date | Author | Change |
|------|--------|--------|
| 2026-05-13 | agent:NotebookLM-orchestrator | Created -- Rev 4 ordered upload checklist for 163-source rebuild. |
