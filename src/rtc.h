#ifndef CLOUD360_RTC_H
#define CLOUD360_RTC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Bounded, allocation-free WebRTC signaling data validation.
 * These routines do NOT establish ICE, DTLS, SRTP or a media connection.
 * No third-party code has been copied into Cloud360. */
enum {
    CLOUD360_RTC_SDP_MAX = 16384,
    CLOUD360_RTC_SDP_LINE_MAX = 512,
    CLOUD360_RTC_SDP_LINES_MAX = 256,
    CLOUD360_RTC_CANDIDATES_MAX = 128,
    CLOUD360_RTC_FOUNDATION_MAX = 33,
    CLOUD360_RTC_ICE_UFRAG_MAX = 257,
    CLOUD360_RTC_ICE_PWD_MAX = 257
};

typedef enum {
    CLOUD360_RTC_CAND_HOST = 0,
    CLOUD360_RTC_CAND_SRFLX,
    CLOUD360_RTC_CAND_PRFLX,
    CLOUD360_RTC_CAND_RELAY
} Cloud360RtcCandidateType;

typedef struct {
    char foundation[CLOUD360_RTC_FOUNDATION_MAX];
    uint8_t component; /* RTP=1, RTCP=2 */
    uint32_t priority;
    uint8_t ipv4[4];
    uint16_t port;
    Cloud360RtcCandidateType type;
    bool has_related_address;
    uint8_t related_ipv4[4];
    uint16_t related_port;
} Cloud360RtcCandidate;

typedef enum {
    CLOUD360_RTC_SETUP_UNKNOWN = 0,
    CLOUD360_RTC_SETUP_ACTIVE,
    CLOUD360_RTC_SETUP_PASSIVE
} Cloud360RtcSetup;

typedef struct {
    char ice_ufrag[CLOUD360_RTC_ICE_UFRAG_MAX];
    char ice_pwd[CLOUD360_RTC_ICE_PWD_MAX];
    uint8_t fingerprint_sha256[32];
    Cloud360RtcSetup setup;
    uint32_t lines;
    uint32_t candidate_lines;
    uint32_t ipv4_udp_candidates;
    uint8_t h264_payload_type;
    bool has_fingerprint;
    bool has_bundle;
    bool has_video;
    bool has_h264;
    bool has_rtcp_mux;
} Cloud360RtcSdp;

/* Parse one "candidate:foundation 1 udp ..." string. IPv4/UDP only:
 * IPv6, TCP and mDNS require explicit future implementations. */
bool cloud360_rtc_candidate_parse(const char *text,
                                   Cloud360RtcCandidate *out);

/* Parse a remote SDP answer and enforce basic ICE + DTLS fingerprint
 * presence, BUNDLE, video H264 and RTCP mux. Extracting a fingerprint
 * is NOT the same as verifying the remote DTLS certificate. */
bool cloud360_rtc_sdp_parse(const char *sdp, size_t length,
                            Cloud360RtcSdp *out);

#endif
