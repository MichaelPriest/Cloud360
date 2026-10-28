#include "../src/rtc.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char *const valid =
    "v=0\r\n"
    "o=- 46117334 2 IN IP4 127.0.0.1\r\n"
    "s=-\r\n"
    "t=0 0\r\n"
    "a=group:BUNDLE 0 1\r\n"
    "a=ice-ufrag:abcdefgh\r\n"
    "a=ice-pwd:ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890\r\n"
    "a=fingerprint:sha-256 "
    "00:01:02:03:04:05:06:07:08:09:0A:0B:0C:0D:0E:0F:"
    "10:11:12:13:14:15:16:17:18:19:1A:1B:1C:1D:1E:1F\r\n"
    "a=setup:active\r\n"
    "m=audio 9 UDP/TLS/RTP/SAVPF 111\r\n"
    "a=mid:1\r\n"
    "a=rtpmap:111 opus/48000/2\r\n"
    "m=video 9 UDP/TLS/RTP/SAVPF 96\r\n"
    "a=mid:0\r\n"
    "a=rtcp-mux\r\n"
    "a=rtpmap:96 H264/90000\r\n"
    "a=candidate:1920046481 1 udp 2122260223 203.0.113.42 54546 typ host generation 0\r\n"
    "a=candidate:abcd 1 udp 1686052863 198.51.100.7 50000 typ srflx "
    "raddr 10.0.0.2 rport 1234\r\n"
    "a=candidate:invalid 1 tcp 1518280447 192.0.2.2 9 typ host\r\n";

static void valid_candidates(void)
{
    Cloud360RtcCandidate c;
    assert(cloud360_rtc_candidate_parse(
        "candidate:12345 1 UDP 2122260223 203.0.113.42 54546 typ host",
        &c));
    assert(c.component == 1);
    assert(c.port == 54546);
    assert(c.priority == 2122260223u);
    assert(c.ipv4[0] == 203 && c.ipv4[3] == 42);
    assert(c.type == CLOUD360_RTC_CAND_HOST);
    assert(!c.has_related_address);

    assert(cloud360_rtc_candidate_parse(
        "candidate:a+BC-42 2 udp 1686052863 198.51.100.1 4000 typ srflx "
        "raddr 10.0.0.2 rport 1234 generation 0 network-id 1", &c));
    assert(c.component == 2 && c.type == CLOUD360_RTC_CAND_SRFLX);
    assert(c.has_related_address);
    assert(c.related_ipv4[0] == 10 && c.related_port == 1234);

    assert(cloud360_rtc_candidate_parse(
        "candidate:456 1 udp 1 192.0.2.1 65535 typ relay", &c));
    assert(c.type == CLOUD360_RTC_CAND_RELAY);
}

static void reject_candidate(const char *text)
{
    Cloud360RtcCandidate c = { 0 };
    c.port = 9999;
    assert(!cloud360_rtc_candidate_parse(text, &c));
    assert(c.port == 9999); /* no partial output */
}

static void invalid_candidates(void)
{
    reject_candidate("a=candidate:foo 1 udp 1 192.0.2.1 1000 typ host");
    reject_candidate("candidate:foo 1 tcp 1 192.0.2.1 1000 typ host");
    reject_candidate("candidate:foo 1 udp 1 2001:db8::1 1000 typ host");
    reject_candidate("candidate:foo 0 udp 1 192.0.2.1 1000 typ host");
    reject_candidate("candidate:foo 3 udp 1 192.0.2.1 1000 typ host");
    reject_candidate("candidate:foo 1 udp 0 192.0.2.1 1000 typ host");
    reject_candidate("candidate:foo 1 udp 4294967296 192.0.2.1 1000 typ host");
    reject_candidate("candidate:foo 1 udp 1 256.0.2.1 1000 typ host");
    reject_candidate("candidate:foo 1 udp 1 192.0.2.1 0 typ host");
    reject_candidate("candidate:foo 1 udp 1 192.0.2.1 65536 typ host");
    reject_candidate("candidate:foo 1 udp 1 192.0.2.1 1000 typ unknown");
    reject_candidate("candidate:foo 1 udp 1 192.0.2.1 1000 typ srflx raddr 10.0.0.1");
    reject_candidate("candidate:foo 1 udp 1 192.0.2.1 1000 typ host generation");
    reject_candidate("candidate:foo 1 udp 1 192.0.2.1 1000 typ host generation \t");
    reject_candidate("candidate:fo!o 1 udp 1 192.0.2.1 1000 typ host");
    reject_candidate("candidate:foo 1 udp 1 192.0.2.1 1000 typ host raddr bad rport 5");
}

static void valid_sdp(void)
{
    Cloud360RtcSdp s;
    assert(cloud360_rtc_sdp_parse(valid, strlen(valid), &s));
    assert(s.has_video && s.has_h264 && s.has_rtcp_mux);
    assert(s.has_bundle && s.has_fingerprint);
    assert(s.setup == CLOUD360_RTC_SETUP_ACTIVE);
    assert(s.h264_payload_type == 96);
    assert(s.fingerprint_sha256[0] == 0 && s.fingerprint_sha256[31] == 31);
    assert(!strcmp(s.ice_ufrag, "abcdefgh"));
    assert(!strcmp(s.ice_pwd, "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890"));
    assert(s.candidate_lines == 3);
    assert(s.ipv4_udp_candidates == 2); /* IPv4 TCP explicitly unsupported */
}

static void bad_sdp(void)
{
    Cloud360RtcSdp s = { 0 };
    assert(!cloud360_rtc_sdp_parse("v=0\r\n", 5, &s));
    assert(!cloud360_rtc_sdp_parse(NULL, 10, &s));
    assert(!cloud360_rtc_sdp_parse(valid, 0, &s));
    assert(!cloud360_rtc_sdp_parse(valid, CLOUD360_RTC_SDP_MAX + 1, &s));

    /* Changed essential fields cannot pass SDP gate. */
    char text[8192] = { 0 };
    assert(strlen(valid) < sizeof text);
    memcpy(text, valid, strlen(valid) + 1);
    char *p = strstr(text, "a=setup:active");
    assert(p);
    memcpy(p, "a=setup:actpas", 14);
    assert(!cloud360_rtc_sdp_parse(text, strlen(text), &s));

    strcpy(text, valid);
    p = strstr(text, "a=rtcp-mux");
    assert(p);
    memcpy(p, "a=rtcp-bad", 10);
    assert(!cloud360_rtc_sdp_parse(text, strlen(text), &s));

    strcpy(text, valid);
    p = strstr(text, "a=ice-ufrag:abcdefgh");
    assert(p);
    memcpy(p, "a=ice-ufrag:abc!efgh", 20);
    assert(!cloud360_rtc_sdp_parse(text, strlen(text), &s));

    strcpy(text, valid);
    p = strstr(text, "00:01:02:03");
    assert(p);
    p[0] = 'Z';
    assert(!cloud360_rtc_sdp_parse(text, strlen(text), &s));

    strcpy(text, valid);
    p = strstr(text, "H264/90000");
    assert(p);
    memcpy(p, "H263/90000", 10);
    assert(!cloud360_rtc_sdp_parse(text, strlen(text), &s));

    strcpy(text, valid);
    p = strstr(text, "a=group:BUNDLE 0 1");
    assert(p);
    memcpy(p, "a=group:OTHER  0 1", 18);
    assert(!cloud360_rtc_sdp_parse(text, strlen(text), &s));

    strcpy(text, valid);
    p = strstr(text, "a=ice-pwd:");
    assert(p);
    /* CR injection in the middle of a field must be rejected. */
    p[13] = '\r';
    assert(!cloud360_rtc_sdp_parse(text, strlen(text), &s));

    /* Do not mutate the last accepted SDP on parse failure. */
    assert(s.has_video == false);
}

int main(void)
{
    valid_candidates();
    invalid_candidates();
    valid_sdp();
    bad_sdp();
    puts("Cloud360 bounded SDP/ICE candidate validation: PASS");
    return 0;
}
