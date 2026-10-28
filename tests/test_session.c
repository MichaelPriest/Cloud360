#include "../src/session.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static Cloud360Session session;
static Cloud360SessionView view;

static uint32_t generation(void)
{
    return session.generation;
}

static void event_at(Cloud360SessionEvent event, uint32_t now)
{
    assert(cloud360_session_event(&session, generation(), event, now, 0, 0, 0));
}

static void test_fail_closed(void)
{
    cloud360_session_init(&session);
    cloud360_session_snapshot(&session, 0, &view);
    assert(view.phase == CLOUD360_SESSION_IDLE);
    assert(!cloud360_session_begin(&session, "test-001", false, true, 100));
    assert(session.phase == CLOUD360_SESSION_NEED_XSTS);
    assert(!cloud360_session_begin(&session, "test-001", true, false, 100));
    assert(session.phase == CLOUD360_SESSION_NO_TRANSPORT);
    assert(!cloud360_session_begin(&session, "bad/injected", true, true, 100));
    assert(session.phase == CLOUD360_SESSION_FAILED);
    assert(!cloud360_session_begin(&session, "", true, true, 100));
    assert(!cloud360_session_begin(&session, "a b", true, true, 100));
    assert(!cloud360_session_begin(&session, NULL, true, true, 100));
    assert(!cloud360_session_event(&session, generation(),
                                   CLOUD360_SESSION_EVT_FIRST_VIDEO,
                                   101, 0, 0, 0));
    cloud360_session_snapshot(&session, 101, &view);
    assert(view.can_retry);
}

static void test_full_lifecycle_and_reconnect(void)
{
    cloud360_session_init(&session);
    assert(cloud360_session_begin(&session, "game-123", true, true, 1000));
    uint32_t gen = generation();
    assert(!cloud360_session_begin(&session, "other", true, true, 1000));
    assert(!cloud360_session_event(&session, gen,
                                   CLOUD360_SESSION_EVT_FIRST_VIDEO,
                                   1010, 0, 0, 0));
    event_at(CLOUD360_SESSION_EVT_CREATED, 1100);
    assert(session.phase == CLOUD360_SESSION_PROVISIONING);
    assert(cloud360_session_poll_due(&session, 1100));
    assert(!cloud360_session_poll_due(&session, 2099));
    assert(cloud360_session_poll_due(&session, 2100));

    assert(cloud360_session_event(&session, gen, CLOUD360_SESSION_EVT_QUEUE,
                                  3000, 7, 95, 0));
    assert(session.phase == CLOUD360_SESSION_QUEUED);
    assert(session.queue_position == 7 && session.queue_eta_seconds == 95);
    assert(cloud360_session_event(&session, gen, CLOUD360_SESSION_EVT_QUEUE,
                                  4000, 4, 60, 0));
    assert(session.queue_position == 4);
    assert(cloud360_session_event(&session, gen,
                                  CLOUD360_SESSION_EVT_PROVISIONING,
                                  5000, 0, 0, 0));
    assert(session.phase == CLOUD360_SESSION_PROVISIONING);
    assert(cloud360_session_event(&session, gen, CLOUD360_SESSION_EVT_READY,
                                  6000, 0, 0, 0));
    assert(session.phase == CLOUD360_SESSION_READY_TO_CONNECT);
    event_at(CLOUD360_SESSION_EVT_SIGNAL_ACCEPTED, 6100);
    assert(session.phase == CLOUD360_SESSION_SIGNALING);
    event_at(CLOUD360_SESSION_EVT_PEER_CONNECTED, 6200);
    assert(session.phase == CLOUD360_SESSION_CONNECTING);
    event_at(CLOUD360_SESSION_EVT_SIGNAL_ACCEPTED, 6300);
    assert(session.phase == CLOUD360_SESSION_WAITING_VIDEO);
    assert(!cloud360_session_event(&session, gen,
                                   CLOUD360_SESSION_EVT_VIDEO_FRAME,
                                   6400, 0, 0, 0));
    event_at(CLOUD360_SESSION_EVT_FIRST_VIDEO, 6500);
    assert(session.phase == CLOUD360_SESSION_STREAMING);

    event_at(CLOUD360_SESSION_EVT_VIDEO_FRAME, 15000);
    assert(!cloud360_session_tick(&session, 26999, true));
    assert(cloud360_session_tick(&session, 27000, true));
    assert(session.phase == CLOUD360_SESSION_RECONNECTING);
    assert(session.reconnect_attempts == 1);
    event_at(CLOUD360_SESSION_EVT_RECONNECT_READY, 27100);
    event_at(CLOUD360_SESSION_EVT_PEER_CONNECTED, 27200);
    event_at(CLOUD360_SESSION_EVT_SIGNAL_ACCEPTED, 27300);
    event_at(CLOUD360_SESSION_EVT_FIRST_VIDEO, 27400);
    assert(session.phase == CLOUD360_SESSION_STREAMING);
    event_at(CLOUD360_SESSION_EVT_CONNECTION_LOST, 27500);
    assert(session.phase == CLOUD360_SESSION_RECONNECTING);
    assert(session.reconnect_attempts == 2);
    event_at(CLOUD360_SESSION_EVT_RECONNECT_READY, 27600);
    event_at(CLOUD360_SESSION_EVT_PEER_CONNECTED, 27700);
    event_at(CLOUD360_SESSION_EVT_SIGNAL_ACCEPTED, 27800);
    event_at(CLOUD360_SESSION_EVT_FIRST_VIDEO, 27900);
    event_at(CLOUD360_SESSION_EVT_CONNECTION_LOST, 28000);
    assert(session.phase == CLOUD360_SESSION_FAILED);
    assert(session.error == CLOUD360_SESSION_ERR_RECONNECT_LIMIT);
    assert(!cloud360_session_event(&session, gen,
                                   CLOUD360_SESSION_EVT_FIRST_VIDEO,
                                   28100, 0, 0, 0));
    cloud360_session_snapshot(&session, 28100, &view);
    assert(view.can_retry);
    assert(!strcmp(cloud360_session_error_text(view.error),
                   "LIMITE DE RECONEXOES"));
    assert(!strcmp(view.title_id, "game-123"));
}

static void test_queue_timeout(void)
{
    cloud360_session_init(&session);
    assert(cloud360_session_begin(&session, "queue", true, true, 0));
    event_at(CLOUD360_SESSION_EVT_QUEUE, 100);
    assert(session.queue_started);
    assert(!cloud360_session_tick(&session, 1800100 - 1, true));
    assert(cloud360_session_tick(&session, 1800100, true));
    assert(session.error == CLOUD360_SESSION_ERR_TIMEOUT);

    cloud360_session_init(&session);
    assert(cloud360_session_begin(&session, "provision", true, true, 0));
    event_at(CLOUD360_SESSION_EVT_CREATED, 0);
    assert(!cloud360_session_tick(&session,
                                  CLOUD360_SESSION_PROVISION_TIMEOUT_MS - 1,
                                  true));
    assert(cloud360_session_tick(&session,
                                 CLOUD360_SESSION_PROVISION_TIMEOUT_MS,
                                 true));
    assert(session.phase == CLOUD360_SESSION_FAILED);

    cloud360_session_init(&session);
    assert(cloud360_session_begin(&session, "request", true, true,
                                  UINT32_MAX - 1000));
    assert(!cloud360_session_tick(&session, 28998, true));
    assert(cloud360_session_tick(&session, 28999, true));
    assert(session.phase == CLOUD360_SESSION_FAILED);
}

static void test_error_cancel_generation(void)
{
    cloud360_session_init(&session);
    assert(cloud360_session_begin(&session, "game", true, true, 100));
    uint32_t old = generation();
    assert(cloud360_session_tick(&session, 101, false));
    assert(session.error == CLOUD360_SESSION_ERR_AUTH_EXPIRED);
    assert(!cloud360_session_event(&session, old,
                                   CLOUD360_SESSION_EVT_CREATED,
                                   102, 0, 0, 0));

    assert(cloud360_session_begin(&session, "game", true, true, 200));
    old = generation();
    cloud360_session_cancel(&session, 201);
    assert(session.phase == CLOUD360_SESSION_STOPPING);
    assert(!cloud360_session_event(&session, old,
                                   CLOUD360_SESSION_EVT_CREATED,
                                   202, 0, 0, 0));
    assert(!cloud360_session_begin(&session, "new", true, true, 203));
    event_at(CLOUD360_SESSION_EVT_STOPPED, 204);
    assert(session.phase == CLOUD360_SESSION_STOPPED);
    assert(!cloud360_session_event(&session, old,
                                   CLOUD360_SESSION_EVT_ERROR,
                                   205, 0, 0, 99));

    assert(cloud360_session_begin(&session, "game", true, true, 300));
    old = generation();
    assert(cloud360_session_event(&session, old,
                                   CLOUD360_SESSION_EVT_ERROR,
                                   301, 0, 0, 403));
    assert(session.phase == CLOUD360_SESSION_FAILED);
    assert(session.server_error == 403);
    cloud360_session_snapshot(&session, 302, &view);
    assert(view.server_error == 403);

    cloud360_session_cancel(&session, 303);
    assert(session.phase == CLOUD360_SESSION_STOPPED);
}

int main(void)
{
    test_fail_closed();
    test_full_lifecycle_and_reconnect();
    test_queue_timeout();
    test_error_cancel_generation();
    puts("Cloud360 queue, signaling, media, reconnect, expiry, cancellation: PASS");
    return 0;
}
