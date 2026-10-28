#ifndef CLOUD360_ICE_AGENT_H
#define CLOUD360_ICE_AGENT_H

#include "ice.h"
#include "udp.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Conservative IPv4/UDP candidate-pair probe scheduler.
 *
 * This does NOT implement full RFC 8445 ICE, a bidirectional checklist,
 * consent freshness, TURN, role conflict recovery or DTLS-SRTP.
 * "VERIFIED" means a single Binding
 * success response passed HMAC and matching UDP peer checks;
 * it MUST NOT be used as proof of an established WebRTC connection.
 *
 * All network and cryptographic operations are injected. The XEX
 * production loop does not instantiate or start this object yet.
 */
enum {
    CLOUD360_ICE_AGENT_PAIRS_MAX = 8,
    CLOUD360_ICE_AGENT_ATTEMPTS_MAX = 5,
    CLOUD360_ICE_AGENT_RTO_INITIAL_MS = 250,
    CLOUD360_ICE_AGENT_RTO_MAX_MS = 4000,
    CLOUD360_ICE_AGENT_PAIR_TIMEOUT_MS = 12000
};

typedef enum {
    CLOUD360_ICE_AGENT_IDLE = 0,
    CLOUD360_ICE_AGENT_CHECKING,
    CLOUD360_ICE_AGENT_VERIFIED_NOT_NOMINATED,
    CLOUD360_ICE_AGENT_NOMINATING,
    CLOUD360_ICE_AGENT_NOMINATION_ACKNOWLEDGED,
    CLOUD360_ICE_AGENT_EXHAUSTED,
    CLOUD360_ICE_AGENT_CANCELLED,
    CLOUD360_ICE_AGENT_CRYPTO_UNAVAILABLE
} Cloud360IceAgentState;

typedef enum {
    CLOUD360_ICE_PAIR_WAITING = 0,
    CLOUD360_ICE_PAIR_INFLIGHT,
    CLOUD360_ICE_PAIR_VERIFIED,
    CLOUD360_ICE_PAIR_FAILED
} Cloud360IcePairState;

/* Real deployment MUST provide a CSPRNG, e.g. a vetted port.
 * No PRNG based on clock, rand() or a gamepad counter is allowed. */
typedef bool (*Cloud360IceRandom)(void *context, uint8_t *data, size_t bytes);

typedef bool (*Cloud360IceSend)(void *context,
                                 const Cloud360UdpPeer *destination,
                                 const uint8_t *packet, size_t bytes);

typedef struct {
    const char *local_ufrag;
    const char *remote_ufrag;
    const char *remote_pwd;
    uint32_t candidate_priority;
    uint64_t tie_breaker;
    bool controlling;
    /* Opt-in only. Full ICE role conflict and consent still required. */
    bool enable_nomination;
} Cloud360IceAgentConfig;

typedef struct {
    Cloud360UdpPeer remote;
    uint32_t priority;
    Cloud360IcePairState state;
    uint8_t transaction_id[CLOUD360_STUN_TRANSACTION_LEN];
    uint32_t started_ms;
    uint32_t next_retry_ms;
    uint32_t retransmit_ms;
    uint8_t attempts;
} Cloud360IcePair;

typedef struct {
    Cloud360IceAgentState state;
    Cloud360IcePair pairs[CLOUD360_ICE_AGENT_PAIRS_MAX];
    uint32_t count;
    uint32_t active_index;
    uint32_t generation;
    uint32_t sends_total;
    uint32_t invalid_responses;
    char local_ufrag[CLOUD360_ICE_UFRAG_MAX + 1];
    char remote_ufrag[CLOUD360_ICE_UFRAG_MAX + 1];
    char remote_password[CLOUD360_ICE_PWD_MAX + 1];
    uint32_t local_priority;
    uint64_t tie_breaker;
    uint32_t verified_at_ms;
    bool controlling;
    bool enable_nomination;
    Cloud360IceHmacSha1 hmac;
    Cloud360IceRandom random;
    Cloud360IceSend send;
    void *context;
} Cloud360IceAgent;

typedef struct {
    Cloud360IceAgentState state;
    Cloud360IcePairState active_pair_state;
    uint32_t generation;
    uint32_t pairs;
    uint32_t active_pair;
    uint32_t sends_total;
    uint32_t invalid_responses;
    uint32_t attempts_for_active_pair;
    bool has_authenticated_response;
    bool nomination_acknowledged;
    /* Deliberately no ICE credentials, tokens or transaction IDs. */
} Cloud360IceAgentView;

void cloud360_ice_agent_init(Cloud360IceAgent *agent);

/* Candidate pairs are supplied by a future validated SDP + local
 * interface adapter; never scraped from an untrusted game catalog.
 * Identical IPv4:port is rejected to prevent duplicate probes. */
bool cloud360_ice_agent_add_pair(Cloud360IceAgent *agent,
                                 Cloud360UdpPeer remote, uint32_t priority);

/* Begin from a validated configuration. Registered callbacks may be
 * asynchronous at the callsite, but these functions themselves must
 * run on ONE owner thread (no implicit locking). */
bool cloud360_ice_agent_start(Cloud360IceAgent *agent,
                               const Cloud360IceAgentConfig *config,
                               Cloud360IceHmacSha1 hmac,
                               Cloud360IceRandom random,
                               Cloud360IceSend send,
                               void *context);

/* Tick on a monotonic millisecond clock. This is the ONLY function
 * which calls send(). Retries reuse their original transaction ID. */
void cloud360_ice_agent_tick(Cloud360IceAgent *agent, uint32_t now_ms);

/* After a successful basic check, the CONTROLLING agent may explicitly
 * nominate the SAME pair with a fresh transaction ID and USE-CANDIDATE.
 * Nothing is sent until the next tick(). The call fails if the prior
 * proof is 30 seconds old. This is opt-in; existing checks stay safe. */
bool cloud360_ice_agent_nominate(Cloud360IceAgent *agent, uint32_t now_ms);

/* Drop wrong-origin, wrong-generation, late or unauthenticated packets.
 * A successful basic check never nominates a pair by itself. */
bool cloud360_ice_agent_receive(Cloud360IceAgent *agent,
                                 uint32_t generation,
                                 Cloud360UdpPeer source,
                                 const uint8_t *packet, size_t size,
                                 uint32_t now_ms);

void cloud360_ice_agent_cancel(Cloud360IceAgent *agent);
void cloud360_ice_agent_view(const Cloud360IceAgent *agent,
                             Cloud360IceAgentView *out);
const char *cloud360_ice_agent_status(Cloud360IceAgentState state);

#endif
