#ifndef CLOUD360_SESSION_H
#define CLOUD360_SESSION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Cloud360 session coordinator for an Xbox 360 XSDK title.
 *
 * The XCloud HTTP, WebRTC and media backends are NOT present yet.
 * Production must call begin(..., transport_available=false) until an
 * independently verified, authorized implementation has been wired in.
 *
 * Each callback includes the generation returned by begin(). Stale
 * worker replies after cancellation or retry cannot revive a session.
 * Never feed credentials into this structure or its display snapshot.
 */
enum {
    CLOUD360_SESSION_TITLE_BYTES = 64,
    CLOUD360_SESSION_MAX_RECONNECTS = 2,
    CLOUD360_SESSION_POLL_INTERVAL_MS = 1000,
    CLOUD360_SESSION_REQUEST_TIMEOUT_MS = 30000,
    CLOUD360_SESSION_PROVISION_TIMEOUT_MS = 210000,
    CLOUD360_SESSION_QUEUE_TIMEOUT_MS = 1800000,
    CLOUD360_SESSION_SIGNAL_TIMEOUT_MS = 60000,
    CLOUD360_SESSION_CONNECT_TIMEOUT_MS = 30000,
    CLOUD360_SESSION_MEDIA_TIMEOUT_MS = 12000,
    CLOUD360_SESSION_RECONNECT_TIMEOUT_MS = 30000,
    CLOUD360_SESSION_STOP_TIMEOUT_MS = 10000
};

typedef enum {
    CLOUD360_SESSION_IDLE = 0,
    CLOUD360_SESSION_NEED_XSTS,
    CLOUD360_SESSION_NO_TRANSPORT,
    CLOUD360_SESSION_REQUESTING,
    CLOUD360_SESSION_PROVISIONING,
    CLOUD360_SESSION_QUEUED,
    CLOUD360_SESSION_READY_TO_CONNECT,
    CLOUD360_SESSION_SIGNALING,
    CLOUD360_SESSION_CONNECTING,
    CLOUD360_SESSION_WAITING_VIDEO,
    CLOUD360_SESSION_STREAMING,
    CLOUD360_SESSION_RECONNECTING,
    CLOUD360_SESSION_STOPPING,
    CLOUD360_SESSION_STOPPED,
    CLOUD360_SESSION_FAILED
} Cloud360SessionPhase;

typedef enum {
    CLOUD360_SESSION_EVT_CREATED = 0,
    CLOUD360_SESSION_EVT_PROVISIONING,
    CLOUD360_SESSION_EVT_QUEUE,
    CLOUD360_SESSION_EVT_READY,
    CLOUD360_SESSION_EVT_SIGNAL_ACCEPTED,
    CLOUD360_SESSION_EVT_PEER_CONNECTED,
    CLOUD360_SESSION_EVT_FIRST_VIDEO,
    CLOUD360_SESSION_EVT_VIDEO_FRAME,
    CLOUD360_SESSION_EVT_CONNECTION_LOST,
    CLOUD360_SESSION_EVT_RECONNECT_READY,
    CLOUD360_SESSION_EVT_STOPPED,
    CLOUD360_SESSION_EVT_ERROR
} Cloud360SessionEvent;

typedef enum {
    CLOUD360_SESSION_ERR_NONE = 0,
    CLOUD360_SESSION_ERR_TIMEOUT,
    CLOUD360_SESSION_ERR_TRANSPORT,
    CLOUD360_SESSION_ERR_AUTH_EXPIRED,
    CLOUD360_SESSION_ERR_RECONNECT_LIMIT,
    CLOUD360_SESSION_ERR_REMOTE
} Cloud360SessionError;

typedef struct {
    Cloud360SessionPhase phase;
    Cloud360SessionError error;
    char title_id[CLOUD360_SESSION_TITLE_BYTES];
    uint32_t generation;
    uint32_t phase_since_ms;
    uint32_t queue_since_ms;
    uint32_t next_poll_ms;
    uint32_t last_video_ms;
    uint32_t queue_position;
    uint32_t queue_eta_seconds;
    uint32_t reconnect_attempts;
    uint32_t server_error;
    bool queue_started;
} Cloud360Session;

typedef struct {
    Cloud360SessionPhase phase;
    Cloud360SessionError error;
    char title_id[CLOUD360_SESSION_TITLE_BYTES];
    uint32_t queue_position;
    uint32_t queue_eta_seconds;
    uint32_t reconnect_attempts;
    uint32_t elapsed_seconds;
    uint32_t server_error;
    uint32_t generation;
    bool can_retry;
    bool requires_media;
} Cloud360SessionView;

void cloud360_session_init(Cloud360Session *session);

/* True only when credentials and an actual cloud service transport exist.
 * Cannot initiate any real connection in the current production build. */
bool cloud360_session_begin(Cloud360Session *session, const char *title_id,
                             bool xsts_authorized, bool transport_available,
                             uint32_t now_ms);

/* Strict, generation-tagged external event. queue_pos/eta only apply to EVT_QUEUE.
 * A streaming state is entered ONLY after EVT_FIRST_VIDEO. */
bool cloud360_session_event(Cloud360Session *session, uint32_t generation,
                             Cloud360SessionEvent event, uint32_t now_ms,
                             uint32_t queue_pos, uint32_t eta_seconds,
                             uint32_t server_error);

/* Poll /state at <=1Hz; the actual provider performs the request off-thread. */
bool cloud360_session_poll_due(Cloud360Session *session, uint32_t now_ms);

/* Returns true if a deadline changed the state. Monotonic uint32 clock
 * differences are safe across rollover for these bounded durations. */
bool cloud360_session_tick(Cloud360Session *session, uint32_t now_ms,
                            bool credentials_still_valid);

void cloud360_session_cancel(Cloud360Session *session, uint32_t now_ms);
void cloud360_session_snapshot(const Cloud360Session *session,
                                uint32_t now_ms, Cloud360SessionView *view);
const char *cloud360_session_phase_text(Cloud360SessionPhase phase);
const char *cloud360_session_error_text(Cloud360SessionError error);

#endif
