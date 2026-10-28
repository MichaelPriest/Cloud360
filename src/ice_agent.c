#include "ice_agent.h"
#include <string.h>

static void wipe(void *ptr, size_t length)
{
    volatile unsigned char *p = ptr;
    while (length--) *p++ = 0;
}
static size_t bounded(const char *s, size_t capacity)
{
    if (!s) return 0;
    size_t count = 0;
    while (count < capacity && s[count]) ++count;
    return count;
}
static bool credential(const char *s, size_t min, size_t max)
{
    const size_t n = bounded(s, max + 1);
    if (n < min || n > max) return false;
    for (size_t i = 0; i < n; ++i) {
        const unsigned char c = (unsigned char)s[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
              (c >= 'A' && c <= 'Z') || c == '+' || c == '/'))
            return false;
    }
    return true;
}
static bool valid_peer(const Cloud360UdpPeer *peer)
{
    if (!peer || !peer->port || peer->ip[0] == 0 ||
        peer->ip[0] >= 224 || (peer->ip[0] == 169 &&
        peer->ip[1] == 254))
        return false;
    return true;
}
static bool peer_equals(const Cloud360UdpPeer *a,
                        const Cloud360UdpPeer *b)
{
    return a->port == b->port && memcmp(a->ip, b->ip, 4) == 0;
}
static bool elapsed(uint32_t now, uint32_t then, uint32_t duration)
{
    return (uint32_t)(now - then) >= duration;
}
static bool due(uint32_t now, uint32_t moment)
{
    return (int32_t)(now - moment) >= 0;
}
static void clear_secrets(Cloud360IceAgent *agent)
{
    wipe(agent->local_ufrag, sizeof agent->local_ufrag);
    wipe(agent->remote_ufrag, sizeof agent->remote_ufrag);
    wipe(agent->remote_password, sizeof agent->remote_password);
    for (uint32_t i = 0; i < agent->count; ++i)
        wipe(agent->pairs[i].transaction_id,
             sizeof agent->pairs[i].transaction_id);
    agent->hmac = NULL;
    agent->random = NULL;
    agent->send = NULL;
    agent->context = NULL;
}
void cloud360_ice_agent_init(Cloud360IceAgent *agent)
{
    if (!agent) return;
    /* For reinitialization of a live object call cancel first. */
    memset(agent, 0, sizeof *agent);
}

bool cloud360_ice_agent_add_pair(Cloud360IceAgent *agent,
                                 Cloud360UdpPeer remote, uint32_t priority)
{
    if (!agent || agent->state != CLOUD360_ICE_AGENT_IDLE ||
        agent->count >= CLOUD360_ICE_AGENT_PAIRS_MAX ||
        !valid_peer(&remote) || !priority) return false;
    for (uint32_t i = 0; i < agent->count; ++i)
        if (peer_equals(&agent->pairs[i].remote, &remote)) return false;
    Cloud360IcePair *p = &agent->pairs[agent->count++];
    p->remote = remote;
    p->priority = priority;
    p->state = CLOUD360_ICE_PAIR_WAITING;
    return true;
}
bool cloud360_ice_agent_start(Cloud360IceAgent *agent,
                               const Cloud360IceAgentConfig *cfg,
                               Cloud360IceHmacSha1 hmac,
                               Cloud360IceRandom random,
                               Cloud360IceSend send, void *context)
{
    if (!agent || !cfg || agent->state != CLOUD360_ICE_AGENT_IDLE ||
        !agent->count || !cfg->candidate_priority ||
        (cfg->enable_nomination && !cfg->controlling) ||
        !credential(cfg->local_ufrag, 4, CLOUD360_ICE_UFRAG_MAX) ||
        !credential(cfg->remote_ufrag, 4, CLOUD360_ICE_UFRAG_MAX) ||
        !credential(cfg->remote_pwd, 22, CLOUD360_ICE_PWD_MAX))
        return false;

    if (!hmac || !random || !send) {
        agent->state = CLOUD360_ICE_AGENT_CRYPTO_UNAVAILABLE;
        ++agent->generation;
        return false;
    }
    memcpy(agent->local_ufrag, cfg->local_ufrag,
           strlen(cfg->local_ufrag) + 1);
    memcpy(agent->remote_ufrag, cfg->remote_ufrag,
           strlen(cfg->remote_ufrag) + 1);
    memcpy(agent->remote_password, cfg->remote_pwd,
           strlen(cfg->remote_pwd) + 1);
    agent->local_priority = cfg->candidate_priority;
    agent->tie_breaker = cfg->tie_breaker;
    agent->controlling = cfg->controlling;
    agent->enable_nomination = cfg->enable_nomination;
    agent->verified_at_ms = 0;
    agent->hmac = hmac;
    agent->random = random;
    agent->send = send;
    agent->context = context;
    agent->state = CLOUD360_ICE_AGENT_CHECKING;
    agent->active_index = 0;
    agent->sends_total = 0;
    agent->invalid_responses = 0;
    ++agent->generation;

    /* Highest candidate priority is attempted first, stable ordering. */
    for (uint32_t i = 1; i < agent->count; ++i) {
        Cloud360IcePair item = agent->pairs[i];
        uint32_t j = i;
        while (j && agent->pairs[j - 1].priority < item.priority) {
            agent->pairs[j] = agent->pairs[j - 1];
            --j;
        }
        agent->pairs[j] = item;
    }
    return true;
}
static void exhausted(Cloud360IceAgent *agent)
{
    agent->state = CLOUD360_ICE_AGENT_EXHAUSTED;
    clear_secrets(agent);
    ++agent->generation;
}
static void failed_pair(Cloud360IceAgent *agent)
{
    Cloud360IcePair *pair = &agent->pairs[agent->active_index];
    pair->state = CLOUD360_ICE_PAIR_FAILED;
    /* Nomination is for one previously validated pair. On failure,
     * do NOT switch destinations and imply the same peer was nominated. */
    if (agent->state == CLOUD360_ICE_AGENT_NOMINATING) {
        exhausted(agent);
        return;
    }
    /* Keep this ID until the entire attempt finishes, so CSPRNG
     * collisions with later candidate checks can be rejected.
     * clear_secrets() wipes every transaction ID on exit. */
    ++agent->active_index;
    if (agent->active_index >= agent->count)
        exhausted(agent);
}
static bool start_pair(Cloud360IceAgent *agent, uint32_t now)
{
    Cloud360IcePair *pair = &agent->pairs[agent->active_index];
    uint8_t transaction[12] = { 0 };
    if (!agent->random(agent->context, transaction, sizeof transaction))
        return false;
    uint8_t nonzero = 0;
    for (size_t i = 0; i < sizeof transaction; ++i)
        nonzero |= transaction[i];
    if (!nonzero) return false;
    /* Check uniqueness against earlier attempts' IDs in this
     * generation to avoid reply confusion between candidate pairs. */
    for (uint32_t i = 0; i < agent->active_index; ++i) {
        if (memcmp(transaction, agent->pairs[i].transaction_id,
                   sizeof transaction) == 0)
            return false;
    }
    if (agent->state == CLOUD360_ICE_AGENT_NOMINATING &&
        memcmp(transaction, pair->transaction_id,
               sizeof transaction) == 0)
        return false;
    memcpy(pair->transaction_id, transaction, sizeof transaction);
    wipe(transaction, sizeof transaction);
    pair->state = CLOUD360_ICE_PAIR_INFLIGHT;
    pair->started_ms = now;
    pair->next_retry_ms = now;
    pair->retransmit_ms = CLOUD360_ICE_AGENT_RTO_INITIAL_MS;
    pair->attempts = 0;
    return true;
}
void cloud360_ice_agent_tick(Cloud360IceAgent *agent, uint32_t now)
{
    if (!agent || (agent->state != CLOUD360_ICE_AGENT_CHECKING &&
                   agent->state != CLOUD360_ICE_AGENT_NOMINATING))
        return;
    while (agent->active_index < agent->count) {
        Cloud360IcePair *pair = &agent->pairs[agent->active_index];
        if (pair->state == CLOUD360_ICE_PAIR_WAITING) {
            if (!start_pair(agent, now)) {
                failed_pair(agent);
                continue;
            }
        }
        if (elapsed(now, pair->started_ms,
                    CLOUD360_ICE_AGENT_PAIR_TIMEOUT_MS) ||
            (pair->attempts >= CLOUD360_ICE_AGENT_ATTEMPTS_MAX &&
             due(now, pair->next_retry_ms))) {
            failed_pair(agent);
            continue;
        }
        if (!due(now, pair->next_retry_ms)) return;
        if (pair->attempts >= CLOUD360_ICE_AGENT_ATTEMPTS_MAX) return;

        Cloud360IceCheck check = {
            .local_ufrag = agent->local_ufrag,
            .remote_ufrag = agent->remote_ufrag,
            .remote_password = agent->remote_password,
            .priority = agent->local_priority,
            .tie_breaker = agent->tie_breaker,
            .controlling = agent->controlling,
            .nominate = agent->state == CLOUD360_ICE_AGENT_NOMINATING
        };
        uint8_t packet[CLOUD360_STUN_PACKET_MAX] = { 0 };
        size_t written = 0;
        const bool valid = cloud360_ice_build_check(
            &check, pair->transaction_id, agent->hmac,
            agent->context, packet, sizeof packet, &written);
        if (!valid || !written ||
            !agent->send(agent->context,&pair->remote,packet,written)) {
            wipe(packet, sizeof packet);
            failed_pair(agent);
            continue;
        }
        wipe(packet, sizeof packet);
        ++pair->attempts;
        ++agent->sends_total;
        pair->next_retry_ms = now + pair->retransmit_ms;
        if (pair->retransmit_ms < CLOUD360_ICE_AGENT_RTO_MAX_MS)
            pair->retransmit_ms *= 2;
        return;
    }
    exhausted(agent);
}
bool cloud360_ice_agent_nominate(Cloud360IceAgent *agent, uint32_t now)
{
    if (!agent || agent->state !=
            CLOUD360_ICE_AGENT_VERIFIED_NOT_NOMINATED ||
        !agent->enable_nomination || !agent->controlling ||
        !agent->hmac || !agent->random || !agent->send ||
        agent->active_index >= agent->count)
        return false;
    if (elapsed(now, agent->verified_at_ms, 30000u)) {
        exhausted(agent);
        return false;
    }
    agent->state = CLOUD360_ICE_AGENT_NOMINATING;
    agent->pairs[agent->active_index].state = CLOUD360_ICE_PAIR_WAITING;
    /* Old proof callback IDs are invalidated, while the previous
     * transaction ID stays until a distinct nomination ID is drawn. */
    ++agent->generation;
    return true;
}

bool cloud360_ice_agent_receive(Cloud360IceAgent *agent,
                                 uint32_t generation, Cloud360UdpPeer source,
                                 const uint8_t *packet, size_t size,
                                 uint32_t now)
{
    if (!agent || generation != agent->generation ||
        (agent->state != CLOUD360_ICE_AGENT_CHECKING &&
         agent->state != CLOUD360_ICE_AGENT_NOMINATING) ||
        agent->active_index >= agent->count || !packet ||
        !size || size > CLOUD360_STUN_PACKET_MAX)
        return false;
    Cloud360IcePair *pair = &agent->pairs[agent->active_index];
    if (pair->state != CLOUD360_ICE_PAIR_INFLIGHT ||
        pair->attempts == 0 || !peer_equals(&source, &pair->remote) ||
        elapsed(now, pair->started_ms,
                CLOUD360_ICE_AGENT_PAIR_TIMEOUT_MS) ||
        (pair->attempts >= CLOUD360_ICE_AGENT_ATTEMPTS_MAX &&
         due(now, pair->next_retry_ms))) {
        ++agent->invalid_responses;
        return false;
    }
    Cloud360StunMapping mapping;
    if (!cloud360_ice_verify_binding_response(
            packet, size, pair->transaction_id,agent->remote_password,
            agent->hmac, agent->context, &mapping)) {
        /* A verified 487 is a peer role conflict, not packet loss.
         * Fail closed here: RFC 8445 requires a role switch, a NEW
         * tie-breaker and reprioritization of the full checklist.
         * This subset cannot perform those actions safely yet. */
        if (cloud360_ice_verify_role_conflict(
                packet,size,pair->transaction_id,agent->remote_password,
                agent->hmac,agent->context)) {
            agent->state=CLOUD360_ICE_AGENT_ROLE_CONFLICT;
            clear_secrets(agent);
            ++agent->generation;
            return true;
        }
        ++agent->invalid_responses;
        return false;
    }
    pair->state = CLOUD360_ICE_PAIR_VERIFIED;
    if (agent->state == CLOUD360_ICE_AGENT_NOMINATING) {
        agent->state = CLOUD360_ICE_AGENT_NOMINATION_ACKNOWLEDGED;
        /* Nomination acknowledged for this check, but no confirmed
         * full ICE checklist, consent, DTLS or game connection. */
        clear_secrets(agent);
        ++agent->generation;
        return true;
    }
    agent->state = CLOUD360_ICE_AGENT_VERIFIED_NOT_NOMINATED;
    agent->verified_at_ms = now;
    if (!agent->enable_nomination) {
        clear_secrets(agent);
        ++agent->generation;
    }
    /* Plain success is NOT nomination or authorization to send media. */
    return true;
}
void cloud360_ice_agent_cancel(Cloud360IceAgent *agent)
{
    if (!agent) return;
    clear_secrets(agent);
    agent->state = CLOUD360_ICE_AGENT_CANCELLED;
    ++agent->generation;
}
void cloud360_ice_agent_view(const Cloud360IceAgent *agent,
                             Cloud360IceAgentView *out)
{
    if (!out) return;
    memset(out, 0, sizeof *out);
    if (!agent) return;
    out->state = agent->state;
    out->generation = agent->generation;
    out->pairs = agent->count;
    out->active_pair = agent->active_index;
    out->sends_total = agent->sends_total;
    out->invalid_responses = agent->invalid_responses;
    if (agent->active_index < agent->count) {
        out->active_pair_state = agent->pairs[agent->active_index].state;
        out->attempts_for_active_pair =
            agent->pairs[agent->active_index].attempts;
    }
    out->has_authenticated_response =
        agent->state == CLOUD360_ICE_AGENT_VERIFIED_NOT_NOMINATED ||
        agent->state == CLOUD360_ICE_AGENT_NOMINATING ||
        agent->state == CLOUD360_ICE_AGENT_NOMINATION_ACKNOWLEDGED;
    out->nomination_acknowledged =
        agent->state == CLOUD360_ICE_AGENT_NOMINATION_ACKNOWLEDGED;
}
const char *cloud360_ice_agent_status(Cloud360IceAgentState state)
{
    switch (state) {
        case CLOUD360_ICE_AGENT_IDLE: return "AGENTE ICE NAO INICIADO";
        case CLOUD360_ICE_AGENT_CHECKING: return "VERIFICANDO CANDIDATOS...";
        case CLOUD360_ICE_AGENT_VERIFIED_NOT_NOMINATED:
            return "STUN AUTENTICADO, ICE NAO CONCLUIDO";
        case CLOUD360_ICE_AGENT_NOMINATING:
            return "NOMEANDO PAR COM USE-CANDIDATE...";
        case CLOUD360_ICE_AGENT_NOMINATION_ACKNOWLEDGED:
            return "NOMEACAO CONFIRMADA, SEM DTLS/CONSENTIMENTO";
        case CLOUD360_ICE_AGENT_ROLE_CONFLICT:
            return "CONFLITO ICE 487: REINICIAR COM OUTRO PAPEL";
        case CLOUD360_ICE_AGENT_EXHAUSTED: return "CANDIDATOS INACESSIVEIS";
        case CLOUD360_ICE_AGENT_CANCELLED: return "VERIFICACOES CANCELADAS";
        case CLOUD360_ICE_AGENT_CRYPTO_UNAVAILABLE:
            return "CRIPTOGRAFIA ICE INDISPONIVEL";
        default: return "ESTADO ICE DESCONHECIDO";
    }
}
