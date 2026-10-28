#include "../src/xbl.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    char json[CLOUD360_XBL_BODY_MAX];
    assert(cloud360_xbl_user_request("fake.OAuth+Token",json,sizeof json));
    assert(strstr(json,"\"RpsTicket\":\"d=fake.OAuth+Token\""));
    assert(strstr(json,"\"RelyingParty\":\"http://auth.xboxlive.com\""));
    assert(strstr(json,"\"TokenType\":\"JWT\""));
    assert(cloud360_xbl_xsts_request("FAKE_XBOX_U_TOKEN",json,sizeof json));
    assert(strstr(json,"\"SandboxId\":\"RETAIL\""));
    assert(strstr(json,"\"UserTokens\":[\"FAKE_XBOX_U_TOKEN\"]"));
    assert(!cloud360_xbl_xsts_request("",json,sizeof json));
    assert(!cloud360_xbl_user_request("ABC\nHeader: injection",json,sizeof json));
    assert(cloud360_xbl_user_request("A\"\\B",json,sizeof json));
    assert(strstr(json,"RpsTicket\":\"d=A\\\"\\\\B\""));
    assert(!cloud360_xbl_xsts_request("TEST",json,15));

    static Cloud360XblReply reply;
    const char *ok="{\"IssueInstant\":\"2026-10-28T00:00:00Z\","
        "\"NotAfter\":\"2026-10-29T00:00:00Z\","
        "\"Token\":\"FAKE_TOKEN\","
        "\"DisplayClaims\":{\"xui\":[{\"uhs\":\"123456789\","
        "\"xid\":\"1\"}]},\"ignored\":[1,2,{\"yes\":true}]}";
    assert(cloud360_xbl_parse_reply(ok,strlen(ok),&reply));
    assert(strcmp(reply.token,"FAKE_TOKEN")==0);
    assert(strcmp(reply.user_hash,"123456789")==0);
    assert(!reply.has_error);

    const char *err="{\"XErr\":2148916238,\"Message\":\"Authorization denied\","
                    "\"Redirect\":\"https://example.invalid\"}";
    assert(cloud360_xbl_parse_reply(err,strlen(err),&reply));
    assert(reply.has_error && reply.error_code==2148916238u);
    assert(!strcmp(cloud360_xbl_error_text(reply.error_code),
                   "CONTA EXIGE PERMISSAO FAMILIAR"));

    const char *missing="{\"Token\":\"AAA\",\"DisplayClaims\":{\"xui\":[]}}";
    assert(!cloud360_xbl_parse_reply(missing,strlen(missing),&reply));
    const char *duplicate="{\"Token\":\"AAA\",\"Token\":\"BBB\","
        "\"DisplayClaims\":{\"xui\":[{\"uhs\":\"55\"}]}}";
    assert(!cloud360_xbl_parse_reply(duplicate,strlen(duplicate),&reply));
    const char *bad="{\"Token\":\"\\uD800\","
        "\"DisplayClaims\":{\"xui\":[{\"uhs\":\"55\"}]}}";
    assert(!cloud360_xbl_parse_reply(bad,strlen(bad),&reply));
    const char *extra="{\"Token\":\"AAA\",\"DisplayClaims\":{\"xui\":[{\"uhs\":\"5\"}]}} trailing";
    assert(!cloud360_xbl_parse_reply(extra,strlen(extra),&reply));

    cloud360_xbl_wipe(&reply,sizeof reply);
    assert(reply.token[0]==0);
    puts("Cloud360 Xbox U-token/XSTS JSON, requests and errors: PASS");
    return 0;
}
