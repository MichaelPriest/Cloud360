#include "rtc.h"

#include <limits.h>
#include <string.h>

static bool dec(const char *s, uint32_t max, uint32_t *result)
{
    if (!s || !*s) return false;
    uint32_t number = 0;
    for (size_t i = 0; s[i]; ++i) {
        const unsigned char c = (unsigned char)s[i];
        if (c < '0' || c > '9') return false;
        const unsigned digit = (unsigned)(c - '0');
        if (number > (max - digit) / 10 || digit > max)
            return false;
        number = number * 10 + digit;
    }
    *result = number;
    return true;
}

static bool ipv4(const char *s, uint8_t out[4])
{
    if (!s || !*s) return false;
    uint8_t address[4] = { 0 };
    unsigned part = 0, digit_count = 0, number = 0;
    for (size_t i = 0;; ++i) {
        const unsigned char c = (unsigned char)s[i];
        if (c >= '0' && c <= '9') {
            if (++digit_count > 3) return false;
            number = number * 10 + (unsigned)(c - '0');
            if (number > 255) return false;
        } else if (c == '.' || c == '\0') {
            if (!digit_count || part >= 4) return false;
            address[part++] = (uint8_t)number;
            number = 0;
            digit_count = 0;
            if (c == '\0') break;
        } else return false;
    }
    if (part != 4) return false;
    memcpy(out, address, 4);
    return true;
}

static bool next_token(const char **cursor, char *out, size_t capacity)
{
    const char *p = *cursor;
    while (*p == ' ') ++p;
    if (!*p) return false;
    size_t n = 0;
    while (p[n] && p[n] != ' ') {
        const unsigned char c = (unsigned char)p[n];
        if (c < 33 || c > 126 || n + 1 >= capacity)
            return false;
        out[n] = (char)c;
        ++n;
    }
    out[n] = '\0';
    *cursor = p + n;
    return true;
}

bool cloud360_rtc_candidate_parse(const char *text,
                                   Cloud360RtcCandidate *out)
{
    if (!text || !out || strncmp(text, "candidate:", 10))
        return false;
    if (strlen(text) > CLOUD360_RTC_SDP_LINE_MAX)
        return false;
    Cloud360RtcCandidate cand = { 0 };
    const char *cursor = text + 10;
    char token[128];
    uint32_t value = 0;

    if (!next_token(&cursor, cand.foundation, sizeof cand.foundation))
        return false;
    for (size_t i = 0; cand.foundation[i]; ++i) {
        unsigned char ch = (unsigned char)cand.foundation[i];
        if (!((ch >= 'A' && ch <= 'Z') ||
              (ch >= 'a' && ch <= 'z') ||
              (ch >= '0' && ch <= '9') ||
              ch == '+' || ch == '/' || ch == '_' || ch == '-'))
            return false;
    }
    if (!next_token(&cursor, token, sizeof token) ||
        !dec(token, 2, &value) || value == 0) return false;
    cand.component = (uint8_t)value;
    if (!next_token(&cursor, token, sizeof token) ||
        (strcmp(token, "udp") && strcmp(token, "UDP")))
        return false;
    if (!next_token(&cursor, token, sizeof token) ||
        !dec(token, UINT32_MAX, &cand.priority) || cand.priority == 0)
        return false;
    if (!next_token(&cursor, token, sizeof token) ||
        !ipv4(token, cand.ipv4)) return false;
    if (!next_token(&cursor, token, sizeof token) ||
        !dec(token, UINT16_MAX, &value) || value == 0) return false;
    cand.port = (uint16_t)value;
    if (!next_token(&cursor, token, sizeof token) ||
        strcmp(token, "typ") != 0 ||
        !next_token(&cursor, token, sizeof token)) return false;
    if (!strcmp(token, "host")) cand.type = CLOUD360_RTC_CAND_HOST;
    else if (!strcmp(token, "srflx")) cand.type = CLOUD360_RTC_CAND_SRFLX;
    else if (!strcmp(token, "prflx")) cand.type = CLOUD360_RTC_CAND_PRFLX;
    else if (!strcmp(token, "relay")) cand.type = CLOUD360_RTC_CAND_RELAY;
    else return false;

    bool related_ip = false, related_port = false;
    while (next_token(&cursor, token, sizeof token)) {
        char value_text[128];
        if (!next_token(&cursor, value_text, sizeof value_text))
            return false;
        if (!strcmp(token, "raddr")) {
            if (related_ip || !ipv4(value_text, cand.related_ipv4))
                return false;
            related_ip = true;
        } else if (!strcmp(token, "rport")) {
            if (related_port || !dec(value_text, UINT16_MAX, &value) ||
                value == 0) return false;
            cand.related_port = (uint16_t)value;
            related_port = true;
        }
    }
    /* Reject trailing invalid/oversized tokens rather than quietly
     * accepting half a candidate. */
    for (const char *p = cursor; *p; ++p)
        if (*p != ' ') return false;
    if (related_ip != related_port) return false;
    cand.has_related_address = related_ip;
    *out = cand;
    return true;
}

static bool same_field(char *destination, size_t capacity,
                       const char *value, size_t min_length,
                       const char *alphabet)
{
    const size_t len = strlen(value);
    if (len < min_length || len >= capacity)
        return false;
    for (size_t i = 0; i < len; ++i) {
        if (!strchr(alphabet, value[i])) return false;
    }
    if (*destination && strcmp(destination, value))
        return false;
    memcpy(destination, value, len + 1);
    return true;
}

static int nibble(unsigned char c)
{
    if (c >= '0' && c <= '9') return (int)(c - '0');
    if (c >= 'a' && c <= 'f') return (int)(c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return (int)(c - 'A' + 10);
    return -1;
}

static bool fingerprint(const char *text, uint8_t bytes[32])
{
    if (strncmp(text, "sha-256 ", 8) && strncmp(text, "SHA-256 ", 8))
        return false;
    text += 8;
    for (size_t i = 0; i < 32; ++i) {
        const int hi = nibble((unsigned char)text[0]);
        const int lo = nibble((unsigned char)text[1]);
        if (hi < 0 || lo < 0) return false;
        bytes[i] = (uint8_t)((hi << 4) | lo);
        text += 2;
        if (i != 31) {
            if (*text++ != ':') return false;
        }
    }
    return *text == '\0';
}

static bool video_map(const char *text, uint8_t *payload)
{
    char number[4] = { 0 };
    size_t i = 0;
    while (text[i] >= '0' && text[i] <= '9' && i < 3) {
        number[i] = text[i];
        ++i;
    }
    if (i == 0 || text[i] != ' ') return false;
    uint32_t value = 0;
    if (!dec(number, 127, &value)) return false;
    if (strcmp(text + i + 1, "H264/90000") != 0 &&
        strcmp(text + i + 1, "h264/90000") != 0)
        return false;
    *payload = (uint8_t)value;
    return true;
}

/* We support only ASCII attributes used in signaling: credentials,
 * SHA-256 fingerprints, setup, H264 and IPv4/UDP candidates. */
bool cloud360_rtc_sdp_parse(const char *sdp, size_t length,
                            Cloud360RtcSdp *out)
{
    if (!sdp || !out || !length || length > CLOUD360_RTC_SDP_MAX)
        return false;

    Cloud360RtcSdp current = { 0 };
    bool in_video = false, found_setup = false, version = false;
    size_t offset = 0;
    const char *const alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    while (offset < length) {
        size_t next = offset;
        while (next < length && sdp[next] != '\n') ++next;
        size_t end = next;
        if (end > offset && sdp[end - 1] == '\r') --end;
        if (end <= offset || end - offset > CLOUD360_RTC_SDP_LINE_MAX ||
            ++current.lines > CLOUD360_RTC_SDP_LINES_MAX)
            return false;
        char line[CLOUD360_RTC_SDP_LINE_MAX + 1];
        const size_t n = end - offset;
        for (size_t i = 0; i < n; ++i) {
            unsigned char ch = (unsigned char)sdp[offset + i];
            if (ch < 32 || ch > 126) return false;
            line[i] = (char)ch;
        }
        line[n] = 0;
        offset = next < length ? next + 1 : next;

        if (!strcmp(line, "v=0")) version = true;
        else if (!strncmp(line, "m=", 2)) {
            in_video = !strncmp(line, "m=video ", 8);
            if (in_video) current.has_video = true;
        } else if (!strncmp(line, "a=ice-ufrag:", 12)) {
            if (!same_field(current.ice_ufrag, sizeof current.ice_ufrag,
                            line + 12, 4, alphabet)) return false;
        } else if (!strncmp(line, "a=ice-pwd:", 10)) {
            if (!same_field(current.ice_pwd, sizeof current.ice_pwd,
                            line + 10, 22, alphabet)) return false;
        } else if (!strncmp(line, "a=fingerprint:", 14)) {
            uint8_t parsed[32] = { 0 };
            if (!fingerprint(line + 14, parsed) ||
                (current.has_fingerprint &&
                 memcmp(current.fingerprint_sha256, parsed, 32)))
                return false;
            memcpy(current.fingerprint_sha256, parsed, 32);
            current.has_fingerprint = true;
        } else if (!strncmp(line, "a=setup:", 8)) {
            Cloud360RtcSetup setup = CLOUD360_RTC_SETUP_UNKNOWN;
            if (!strcmp(line + 8, "active"))
                setup = CLOUD360_RTC_SETUP_ACTIVE;
            else if (!strcmp(line + 8, "passive"))
                setup = CLOUD360_RTC_SETUP_PASSIVE;
            if (!setup || (found_setup && current.setup != setup))
                return false;
            current.setup = setup;
            found_setup = true;
        } else if (!strncmp(line, "a=group:BUNDLE ", 15)) {
            if (!line[15]) return false;
            current.has_bundle = true;
        } else if (in_video && !strcmp(line, "a=rtcp-mux")) {
            current.has_rtcp_mux = true;
        } else if (in_video && !strncmp(line, "a=rtpmap:", 9)) {
            uint8_t pt = 0;
            if (video_map(line + 9, &pt)) {
                current.has_h264 = true;
                current.h264_payload_type = pt;
            }
        } else if (!strncmp(line, "a=candidate:", 12)) {
            if (++current.candidate_lines > CLOUD360_RTC_CANDIDATES_MAX)
                return false;
            Cloud360RtcCandidate candidate;
            if (cloud360_rtc_candidate_parse(line + 2, &candidate))
                ++current.ipv4_udp_candidates;
        }
    }
    if (!version || !current.has_bundle || !current.has_video ||
        !current.has_h264 || !current.has_rtcp_mux ||
        !current.has_fingerprint || !current.setup ||
        !current.ice_ufrag[0] || !current.ice_pwd[0])
        return false;
    *out = current;
    return true;
}
