static inline bool old_requires_full(const CPUState* cpu, u32 address) {
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
    if ((g_module1_raw_base != 0u && address == g_module1_raw_base + 0xD4u) ||
        bluewake_edge_maybe_intercept(host_canonical_linked_pc(address)))
        return true;
    if ((cpu->msr & PPC_MSR_EE) != 0u &&
        (g_guest_decrementer_pending ||
         (g_interrupts.pi_cause & g_interrupts.pi_mask) != 0u))
        return true;
    return false;
}
static inline bool new_requires_full(const CPUState* cpu, u32 address) {
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
