#include "../src/oauth_json.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    const char *device_json=
        "{\"device_code\":\"my-secret-+\",\"user_code\":\"ABCD-1234\","
        "\"verification_uri\":\"https:\\/\\/microsoft.com\\/devicelogin\","
        "\"expires_in\":900,\"interval\":5,\"message\":\"Use your phone\","
        "\"other\":{\"arr\":[1,true,null]}}";
    Cloud360OAuthDevice d;
    assert(cloud360_oauth_parse_device_json(device_json,strlen(device_json),&d));
    assert(!strcmp(d.device_code,"my-secret-+"));
    assert(!strcmp(d.user_code,"ABCD-1234"));
    assert(!strcmp(d.verification_uri,"https://microsoft.com/devicelogin"));
    assert(d.expires_in_seconds==900 && d.interval_seconds==5);

    const char *missing="{\"user_code\":\"ABC\"}";
    assert(!cloud360_oauth_parse_device_json(missing,strlen(missing),&d));
    const char *duplicate="{\"user_code\":\"ABC\",\"user_code\":\"DEF\"}";
    assert(!cloud360_oauth_parse_device_json(duplicate,strlen(duplicate),&d));
    const char *injection="{\"user_code\":\"A\\nB\",\"device_code\":\"x\","
        "\"verification_uri\":\"https://example.com/\",\"expires_in\":900}";
    assert(!cloud360_oauth_parse_device_json(injection,strlen(injection),&d));
    const char *overflow="{\"device_code\":\"x\",\"user_code\":\"ABC\","
        "\"verification_uri\":\"https://example.com/\",\"expires_in\":4294967296}";
    assert(!cloud360_oauth_parse_device_json(overflow,strlen(overflow),&d));
    const char *trailing="{\"device_code\":\"x\",\"user_code\":\"ABC\","
        "\"verification_uri\":\"https://example.com/\",\"expires_in\":900}garbage";
    assert(!cloud360_oauth_parse_device_json(trailing,strlen(trailing),&d));

    Cloud360OAuthTokenResponse t;
    const char *pending="{\"error\":\"authorization_pending\","
                         "\"error_description\":\"Waiting\"}";
    assert(cloud360_oauth_parse_token_json(pending,strlen(pending),&t));
    assert(t.reply==CLOUD360_OAUTH_REPLY_PENDING);
    assert(t.access_token[0]==0);
    const char *slow="{\"error\":\"slow_down\"}";
    assert(cloud360_oauth_parse_token_json(slow,strlen(slow),&t));
    assert(t.reply==CLOUD360_OAUTH_REPLY_SLOW_DOWN);
    const char *denied="{\"error\":\"authorization_declined\"}";
    assert(cloud360_oauth_parse_token_json(denied,strlen(denied),&t));
    assert(t.reply==CLOUD360_OAUTH_REPLY_DENIED);
    const char *expired="{\"error\":\"expired_token\"}";
    assert(cloud360_oauth_parse_token_json(expired,strlen(expired),&t));
    assert(t.reply==CLOUD360_OAUTH_REPLY_EXPIRED);
    const char *approved="{\"token_type\":\"Bearer\",\"access_token\":"
        "\"eyJFAKE.TEST\",\"refresh_token\":\"opaque-refresh\",\"expires_in\":3600}";
    assert(cloud360_oauth_parse_token_json(approved,strlen(approved),&t));
    assert(t.reply==CLOUD360_OAUTH_REPLY_APPROVED);
    assert(!strcmp(t.access_token,"eyJFAKE.TEST"));
    assert(!strcmp(t.refresh_token,"opaque-refresh"));
    const char *wrong_type="{\"token_type\":\"basic\",\"access_token\":\"not-good\"}";
    assert(!cloud360_oauth_parse_token_json(wrong_type,strlen(wrong_type),&t));
    const char *no_token="{\"token_type\":\"Bearer\"}";
    assert(!cloud360_oauth_parse_token_json(no_token,strlen(no_token),&t));
    puts("Cloud360 bounded OAuth device and token JSON: PASS");
    return 0;
}
