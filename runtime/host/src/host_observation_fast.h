/* Sufficient-negative host proof. False means the unchanged complete predicate.
 * The query never writes CPU, RAM or host state; no answers are cached. */
#ifndef BLUEWAKE_HOST_OBSERVATION_FAST_H
#define BLUEWAKE_HOST_OBSERVATION_FAST_H
#include "game_event_observation_view.h"
#include "host_observation_domains.h"
/* Only a successful source/domain certificate check may enable this path.
 * Unsupported build routes remain on the original callback. */
#ifdef BLUEWAKE_MAINCODE_OBSERVATION_CERTIFICATE_AVAILABLE
#include "host_observation_certificate.h"
#endif
#ifndef BLUEWAKE_MAINCODE_OBSERVATION_CERTIFIED
#define BLUEWAKE_MAINCODE_OBSERVATION_CERTIFIED 0
#endif
/* Process-lifetime host event storage; contents are always read fresh.
 * NULL before registration and after teardown means ordinary fallback. */
static const BwGameEventObservationView* g_host_event_observation_view;
static inline bool host_maincode_can_skip_observation(const CPUState* cpu,u32 address) {
#if !BLUEWAKE_MAINCODE_OBSERVATION_CERTIFIED || BLUEWAKE_ENABLE_DEVELOPER_TRACING || BLUEWAKE_EDGE_CENSUS || defined(BW_NATIVE_REWARD_SESSION) || defined(BW_NATIVE_INVENTORY_COLLECTOR)
    (void)cpu;(void)address;return false;
#else
    if(cpu==NULL) return false;
    /* Raw main-code only: no canonicalization admits mirrors or REL tags. */
    if(!bw_host_observation_code_member(address)) return false;
    if(bw_host_observation_static_excluded(address)) return false;
    /* All finite-family true addresses are already in the static union. */
    if(g_direct_call_trace || g_deadline_census_enabled ||
       g_delivery_safety_census_enabled || g_guest_state_trace_enabled ||
       bluewake_jump_button_armed || g_turn_census_enabled ||
       g_boundary_census_enabled || g_chassis_service_each_block)
        return false;
    if(g_interrupt_sources_dirty || ((cpu->msr&PPC_MSR_EE)!=0u &&
       (g_guest_decrementer_pending || (g_interrupts.pi_cause&g_interrupts.pi_mask)!=0u)))
        return false;
    const BwGameEventObservationView* e=g_host_event_observation_view;
    if(e==NULL) return false;
    if(e->owner_cpu!=NULL && e->mask!=0u) {
        const u32 bucket=(address*UINT32_C(0x9E3779B1))>>24;
        if(e->pending_return_buckets[bucket>>6]&(UINT64_C(1)<<(bucket&63u)))
            return false;
    }
    if(g_overlap_observation && g_name_scene_object>=0x80000000u &&
       (g_file_start_pulse.triggered || !g_file_start_pulse.configured)) {
        if(g_ppc_guest_alias_generation!=g_overlap_cached_alias_state ||
           g_overlap_slot_ptr==NULL) return false;
        const u32 object=read_be32(g_overlap_slot_ptr);
        if(object>=0x80000000u) {
            if(object!=g_overlap_cached_object || g_overlap_fields_ptr==NULL)
                return false;
            if(read_be16(g_overlap_fields_ptr+0x04u)==1u &&
               read_be32(g_overlap_fields_ptr+0x1Cu)!=g_overlap_last_phase)
                return false;
        }
    }
    if(g_module1_raw_base!=0u && address==g_module1_raw_base+0xD4u)
        return false;
    return true;
#endif
}
#endif
