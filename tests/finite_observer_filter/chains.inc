static bool old_chain(void* user, const CPUState* cpu, u32 address) {
    (void)user;
#if BLUEWAKE_ENABLE_DEVELOPER_TRACING || BLUEWAKE_EDGE_CENSUS
    (void)cpu; (void)address;
    return false;
#else
    const bool allowed = !g_deadline_census_enabled && !g_delivery_safety_census_enabled &&
           !g_guest_state_trace_enabled && !bluewake_jump_button_armed &&
           !bluewake_feature_observes(address) &&
           !bluewake_game_events_observes(address) &&
           !bw_hud_host_observes(cpu,address) &&
           !bw_health_host_observes(cpu,address) &&
#ifdef BW_NATIVE_REWARD_SESSION
           !(g_reward_host && bw_randomizer_reward_host_observes(g_reward_host, cpu, address)) &&
#endif
           !bluewake_quick_items_observes(address) &&
           !bluewake_dialogue_speed_observes(address) &&
           !bluewake_enhancement_hooks_observes_context(cpu, address) &&
           !bluewake_autosave_observes(address) &&
           !(address == BW_SEARCH_JUDGE_FILTER && g_actor_search_native) &&
           !host_chassis_requires_full(cpu, address);
    return allowed;
#endif
}
static bool filtered_chain(void* user, const CPUState* cpu, u32 address) {
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
           !host_chassis_requires_full(cpu, address);
    return allowed;
#endif
}
