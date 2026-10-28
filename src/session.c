#include "session.h"

#include <string.h>

static bool phase_active(Cloud360SessionPhase phase)
{
    return phase >= CLOUD360_SESSION_REQUESTING &&
           phase <= CLOUD360_SESSION_STOPPING;
}

static bool valid_title(const char *title)
{
    if (!title) return false;
    size_t n = 0;
    for (; title[n] && n < CLOUD360_SESSION_TITLE_BYTES; ++n) {
        unsigned char c = (unsigned char)title[n];
        if (!((c >= 'a' && c <= 'z') ||
              (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') ||
              c == '_' || c == '-'))
            return false;
    }
    return n > 0 && n < CLOUD360_SESSION_TITLE_BYTES;
}

static bool age_at_least(uint32_t now, uint32_t then, uint32_t duration)
{
    return (uint32_t)(now - then) >= duration;
}

static void transition(Cloud360Session *session, Cloud360SessionPhase phase,
                       uint32_t now)
{
    session->phase = phase;
    session->phase_since_ms = now;
    session->next_poll_ms = now;
}

static void fail(Cloud360Session *session, Cloud360SessionError error,
                 uint32_t now)
{
    session->error = error;
    session->queue_position = 0;
    session->queue_eta_seconds = 0;
    transition(session, CLOUD360_SESSION_FAILED, now);
    ++session->generation; /* invalidate in-flight operation replies */
}

void cloud360_session_init(Cloud360Session *session)
{
    if (session)
        memset(session, 0, sizeof *session);
}

bool cloud360_session_begin(Cloud360Session *session, const char *title_id,
                             bool xsts_authorized, bool transport_available,
                             uint32_t now)
{
    if (!session || phase_active(session->phase))
        return false;

    /* Never pretend to launch a Game Pass title from a local JSON fixture. */
    ++session->generation;
    session->error = CLOUD360_SESSION_ERR_NONE;
    session->server_error = 0;
    session->queue_position = 0;
    session->queue_eta_seconds = 0;
    session->queue_started = false;
    session->reconnect_attempts = 0;
    session->title_id[0] = '\0';

    if (!xsts_authorized) {
        transition(session, CLOUD360_SESSION_NEED_XSTS, now);
        return false;
    }
    if (!transport_available) {
        transition(session, CLOUD360_SESSION_NO_TRANSPORT, now);
        return false;
    }
    if (!valid_title(title_id)) {
        fail(session, CLOUD360_SESSION_ERR_REMOTE, now);
        return false;
    }

    const size_t n = strlen(title_id);
    memcpy(session->title_id, title_id, n + 1);
    transition(session, CLOUD360_SESSION_REQUESTING, now);
    return true;
}

static void reconnect_or_fail(Cloud360Session *session, uint32_t now)
{
    if (session->reconnect_attempts >= CLOUD360_SESSION_MAX_RECONNECTS) {
        fail(session, CLOUD360_SESSION_ERR_RECONNECT_LIMIT, now);
        return;
    }
    ++session->reconnect_attempts;
    transition(session, CLOUD360_SESSION_RECONNECTING, now);
}

bool cloud360_session_event(Cloud360Session *session, uint32_t generation,
                             Cloud360SessionEvent event, uint32_t now,
                             uint32_t queue_pos, uint32_t eta_seconds,
                             uint32_t server_error)
{
    if (!session || generation != session->generation)
        return false;

    if (event == CLOUD360_SESSION_EVT_ERROR && phase_active(session->phase)) {
        session->server_error = server_error;
        fail(session, CLOUD360_SESSION_ERR_REMOTE, now);
        return true;
    }
    if (event == CLOUD360_SESSION_EVT_STOPPED &&
        session->phase == CLOUD360_SESSION_STOPPING) {
        transition(session, CLOUD360_SESSION_STOPPED, now);
        ++session->generation;
        return true;
    }

    switch (session->phase) {
        case CLOUD360_SESSION_REQUESTING:
            if (event == CLOUD360_SESSION_EVT_CREATED ||
                event == CLOUD360_SESSION_EVT_PROVISIONING) {
                transition(session, CLOUD360_SESSION_PROVISIONING, now);
                return true;
            }
            if (event == CLOUD360_SESSION_EVT_QUEUE) {
                session->queue_started = true;
                session->queue_since_ms = now;
                session->queue_position = queue_pos;
                session->queue_eta_seconds = eta_seconds;
                transition(session, CLOUD360_SESSION_QUEUED, now);
                return true;
            }
            break;
        case CLOUD360_SESSION_PROVISIONING:
            if (event == CLOUD360_SESSION_EVT_QUEUE) {
                if (!session->queue_started) {
                    session->queue_started = true;
                    session->queue_since_ms = now;
                }
                session->queue_position = queue_pos;
                session->queue_eta_seconds = eta_seconds;
                transition(session, CLOUD360_SESSION_QUEUED, now);
                return true;
            }
            if (event == CLOUD360_SESSION_EVT_READY) {
                transition(session, CLOUD360_SESSION_READY_TO_CONNECT, now);
                return true;
            }
            break;
        case CLOUD360_SESSION_QUEUED:
            if (event == CLOUD360_SESSION_EVT_QUEUE) {
                session->queue_position = queue_pos;
                session->queue_eta_seconds = eta_seconds;
                return true;
            }
            if (event == CLOUD360_SESSION_EVT_PROVISIONING) {
                transition(session, CLOUD360_SESSION_PROVISIONING, now);
                return true;
            }
            if (event == CLOUD360_SESSION_EVT_READY) {
                transition(session, CLOUD360_SESSION_READY_TO_CONNECT, now);
                return true;
            }
            break;
        case CLOUD360_SESSION_READY_TO_CONNECT:
            if (event == CLOUD360_SESSION_EVT_SIGNAL_ACCEPTED) {
                transition(session, CLOUD360_SESSION_SIGNALING, now);
                return true;
            }
            break;
        case CLOUD360_SESSION_SIGNALING:
            if (event == CLOUD360_SESSION_EVT_PEER_CONNECTED) {
                transition(session, CLOUD360_SESSION_CONNECTING, now);
                return true;
            }
            break;
        case CLOUD360_SESSION_CONNECTING:
            if (event == CLOUD360_SESSION_EVT_SIGNAL_ACCEPTED) {
                transition(session, CLOUD360_SESSION_WAITING_VIDEO, now);
                return true;
            }
            break;
        case CLOUD360_SESSION_WAITING_VIDEO:
            if (event == CLOUD360_SESSION_EVT_FIRST_VIDEO) {
                session->last_video_ms = now;
                transition(session, CLOUD360_SESSION_STREAMING, now);
                return true;
            }
            if (event == CLOUD360_SESSION_EVT_CONNECTION_LOST) {
                reconnect_or_fail(session, now);
                return true;
            }
            break;
        case CLOUD360_SESSION_STREAMING:
            if (event == CLOUD360_SESSION_EVT_VIDEO_FRAME) {
                session->last_video_ms = now;
                return true;
            }
            if (event == CLOUD360_SESSION_EVT_CONNECTION_LOST) {
                reconnect_or_fail(session, now);
                return true;
            }
            break;
        case CLOUD360_SESSION_RECONNECTING:
            if (event == CLOUD360_SESSION_EVT_RECONNECT_READY) {
                transition(session, CLOUD360_SESSION_SIGNALING, now);
                return true;
            }
            break;
        default: break;
    }
    return false;
}

bool cloud360_session_poll_due(Cloud360Session *session, uint32_t now)
{
    if (!session || (session->phase != CLOUD360_SESSION_PROVISIONING &&
                     session->phase != CLOUD360_SESSION_QUEUED))
        return false;
    if ((int32_t)(now - session->next_poll_ms) < 0)
        return false;
    session->next_poll_ms = now + CLOUD360_SESSION_POLL_INTERVAL_MS;
    return true;
}

bool cloud360_session_tick(Cloud360Session *session, uint32_t now,
                            bool credentials_still_valid)
{
    if (!session || !phase_active(session->phase))
        return false;
    if (session->phase != CLOUD360_SESSION_STOPPING &&
        !credentials_still_valid) {
        fail(session, CLOUD360_SESSION_ERR_AUTH_EXPIRED, now);
        return true;
    }
    if (session->queue_started &&
        (session->phase == CLOUD360_SESSION_QUEUED ||
         session->phase == CLOUD360_SESSION_PROVISIONING) &&
        age_at_least(now, session->queue_since_ms,
                     CLOUD360_SESSION_QUEUE_TIMEOUT_MS)) {
        fail(session, CLOUD360_SESSION_ERR_TIMEOUT, now);
        return true;
    }

    uint32_t timeout = 0;
    switch (session->phase) {
        case CLOUD360_SESSION_REQUESTING:
            timeout = CLOUD360_SESSION_REQUEST_TIMEOUT_MS; break;
        case CLOUD360_SESSION_PROVISIONING:
            /* Only provisioning time, without an active queue, is capped. */
            timeout = session->queue_started ? 0 :
                CLOUD360_SESSION_PROVISION_TIMEOUT_MS; break;
        case CLOUD360_SESSION_READY_TO_CONNECT:
        case CLOUD360_SESSION_CONNECTING:
        case CLOUD360_SESSION_RECONNECTING:
            timeout = CLOUD360_SESSION_CONNECT_TIMEOUT_MS; break;
        case CLOUD360_SESSION_SIGNALING:
            timeout = CLOUD360_SESSION_SIGNAL_TIMEOUT_MS; break;
        case CLOUD360_SESSION_WAITING_VIDEO:
            timeout = CLOUD360_SESSION_MEDIA_TIMEOUT_MS; break;
        case CLOUD360_SESSION_STREAMING:
            if (age_at_least(now, session->last_video_ms,
                             CLOUD360_SESSION_MEDIA_TIMEOUT_MS)) {
                reconnect_or_fail(session, now);
                return true;
            }
            break;
        case CLOUD360_SESSION_STOPPING:
            timeout = CLOUD360_SESSION_STOP_TIMEOUT_MS; break;
        default: break;
    }
    if (timeout && age_at_least(now, session->phase_since_ms, timeout)) {
        fail(session, CLOUD360_SESSION_ERR_TIMEOUT, now);
        return true;
    }
    return false;
}

void cloud360_session_cancel(Cloud360Session *session, uint32_t now)
{
    if (!session) return;
    ++session->generation;
    session->error = CLOUD360_SESSION_ERR_NONE;
    session->queue_position = 0;
    session->queue_eta_seconds = 0;
    if (phase_active(session->phase) &&
        session->phase != CLOUD360_SESSION_STOPPING)
        transition(session, CLOUD360_SESSION_STOPPING, now);
    else
        transition(session, CLOUD360_SESSION_STOPPED, now);
    /* Transport adapter must dispatch DELETE with its own safe credentials;
     * the UI never tries a synchronous HTTP call on the render thread. */
}

void cloud360_session_snapshot(const Cloud360Session *session,
                                uint32_t now, Cloud360SessionView *view)
{
    if (!view) return;
    memset(view, 0, sizeof *view);
    if (!session) return;
    view->phase = session->phase;
    view->error = session->error;
    memcpy(view->title_id, session->title_id, sizeof view->title_id);
    view->queue_position = session->queue_position;
    view->queue_eta_seconds = session->queue_eta_seconds;
    view->reconnect_attempts = session->reconnect_attempts;
    view->server_error = session->server_error;
    view->generation = session->generation;
    view->can_retry = session->phase == CLOUD360_SESSION_FAILED ||
                      session->phase == CLOUD360_SESSION_STOPPED ||
                      session->phase == CLOUD360_SESSION_NO_TRANSPORT ||
                      session->phase == CLOUD360_SESSION_NEED_XSTS;
    view->requires_media = session->phase == CLOUD360_SESSION_WAITING_VIDEO ||
                           session->phase == CLOUD360_SESSION_STREAMING;
    if (phase_active(session->phase))
        view->elapsed_seconds = (uint32_t)(now - session->phase_since_ms) / 1000;
}

const char *cloud360_session_phase_text(Cloud360SessionPhase phase)
{
    switch (phase) {
        case CLOUD360_SESSION_IDLE: return "NENHUMA SESSAO SOLICITADA";
        case CLOUD360_SESSION_NEED_XSTS: return "AUTENTIQUE-SE NA ABA CONTA";
        case CLOUD360_SESSION_NO_TRANSPORT: return "PROVEDOR XCLOUD NAO INTEGRADO";
        case CLOUD360_SESSION_REQUESTING: return "SOLICITANDO SESSAO...";
        case CLOUD360_SESSION_PROVISIONING: return "PREPARANDO SERVIDOR...";
        case CLOUD360_SESSION_QUEUED: return "AGUARDANDO NA FILA...";
        case CLOUD360_SESSION_READY_TO_CONNECT: return "SERVIDOR DISPONIVEL";
        case CLOUD360_SESSION_SIGNALING: return "NEGOCIANDO SINALIZACAO...";
        case CLOUD360_SESSION_CONNECTING: return "CONECTANDO WEBRTC...";
        case CLOUD360_SESSION_WAITING_VIDEO: return "AGUARDANDO PRIMEIRO VIDEO...";
        case CLOUD360_SESSION_STREAMING: return "VIDEO EM EXECUCAO";
        case CLOUD360_SESSION_RECONNECTING: return "RESTAURANDO CONEXAO...";
        case CLOUD360_SESSION_STOPPING: return "ENCERRANDO SESSAO...";
        case CLOUD360_SESSION_STOPPED: return "SESSAO ENCERRADA";
        case CLOUD360_SESSION_FAILED: return "SESSAO FALHOU";
        default: return "ESTADO DESCONHECIDO";
    }
}
const char *cloud360_session_error_text(Cloud360SessionError error)
{
    switch (error) {
        case CLOUD360_SESSION_ERR_NONE: return "SEM ERROS";
        case CLOUD360_SESSION_ERR_TIMEOUT: return "LIMITE DE TEMPO EXCEDIDO";
        case CLOUD360_SESSION_ERR_TRANSPORT: return "TRANSPORTE INDISPONIVEL";
        case CLOUD360_SESSION_ERR_AUTH_EXPIRED: return "AUTORIZACAO EXPIRADA";
        case CLOUD360_SESSION_ERR_RECONNECT_LIMIT: return "LIMITE DE RECONEXOES";
        case CLOUD360_SESSION_ERR_REMOTE: return "FALHA INFORMADA PELO SERVIDOR";
        default: return "FALHA DESCONHECIDA";
    }
}
