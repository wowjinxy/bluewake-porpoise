/* SPDX-License-Identifier: GPL-3.0-or-later
 * Verbatim pre-contract host predicates, with only their names changed.
 * Original main.c SHA256 d7ddabbf5104e910e7b122a4cd4d0c18db007f01d18680f27d16b12520833a96. */
static inline bool reference_requires_full(const CPUState* cpu, u32 address) {
    if (__builtin_expect(cpu == NULL || g_turn_census_enabled ||
                             g_boundary_census_enabled ||
                             g_chassis_service_each_block ||
                             g_interrupt_sources_dirty,
                         0))
        return true;
    if (g_overlap_observation && g_name_scene_object >= 0x80000000u &&
        (g_file_start_pulse.triggered || !g_file_start_pulse.configured)) {
        if (g_ppc_guest_alias_generation != g_overlap_cached_alias_state ||
            g_overlap_slot_ptr == NULL)
            return true;
        const u32 object = read_be32(g_overlap_slot_ptr);
        if (object >= 0x80000000u) {
            if (object != g_overlap_cached_object || g_overlap_fields_ptr == NULL)
                return true;
            if (read_be16(g_overlap_fields_ptr + 0x04u) == 1u &&
                read_be32(g_overlap_fields_ptr + 0x1Cu) != g_overlap_last_phase)
                return true;
        }
    }
    if (!g_new_game_intro_reported &&
        (address == 0x80018554u || address == 0x8001199Cu))
        return true;
    // The address first: a compare with an immediate, where the flag is a load.
    if (__builtin_expect(address == 0x80122D30u, 0) && g_player_route_waiting)
        return true;
    // GroundCross observes this shared restore entry only for its own return.
    // Retain the conservative canonical-address match for mirrored entries.
    const u32 canonical_address = host_canonical_linked_pc(address);
    if ((g_module1_raw_base != 0u && address == g_module1_raw_base + 0xD4u) ||
        (bluewake_edge_maybe_intercept(canonical_address) &&
         (canonical_address != 0x80328F84u || cpu->lr == 0x80246A04u)))
        return true;
    if ((cpu->msr & PPC_MSR_EE) != 0u &&
        (g_guest_decrementer_pending ||
         (g_interrupts.pi_cause & g_interrupts.pi_mask) != 0u))
        return true;
    return false;
}

static bool reference_can_skip(void* user, const CPUState* cpu, u32 address) {
    (void)user;
#if BLUEWAKE_ENABLE_DEVELOPER_TRACING || BLUEWAKE_EDGE_CENSUS
    (void)cpu; (void)address;
    return false;
#else
    const bool finite_observer = bluewake_finite_observer_maybe(address);
    const bool allowed = !g_deadline_census_enabled && !g_delivery_safety_census_enabled &&
           !g_guest_state_trace_enabled && !bluewake_jump_button_armed &&
           !bluewake_feature_observes(address) &&
           !bluewake_game_events_observes(address) &&
           (!finite_observer || (!bw_hud_host_observes(cpu,address) &&
                                 !bw_health_host_observes(cpu,address))) &&
#ifdef BW_NATIVE_REWARD_SESSION
           !(g_reward_host && bw_randomizer_reward_host_observes(g_reward_host, cpu, address)) &&
#endif
           (!finite_observer || (!bluewake_quick_items_observes(address) &&
                                 !bluewake_dialogue_speed_observes(address) &&
                                 !bluewake_enhancement_hooks_observes_context(cpu, address) &&
                                 !bluewake_autosave_observes(address))) &&
           !(address == BW_SEARCH_JUDGE_FILTER && g_actor_search_native) &&
           !reference_requires_full(cpu, address);
    return allowed;
#endif
}
