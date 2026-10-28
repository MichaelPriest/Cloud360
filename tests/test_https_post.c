#include "../src/https.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>

int main(void)
{
    Cloud360HttpsRequest req={
        .host="login.microsoftonline.com",
        .path="/consumers/oauth2/v2.0/devicecode",
        .ca_file="/tmp/cloud360-missing-ca-test.pem"
    };
    char result[256]={0};
    Cloud360HttpsBody body={result,sizeof result,0};
    long status=1;
    size_t bytes=1;
    int error=0;
    const char *form="client_id=12345678-1234-1234-1234-123456789abc&scope=openid";
    Cloud360HttpsStatus r=cloud360_https_curl_post_form(
        NULL,&req,form,&body,&status,&bytes,&error);
#ifdef CLOUD360_WITH_CURL
    assert(r==CLOUD360_HTTPS_CA_MISSING);
#else
    assert(r==CLOUD360_HTTPS_UNAVAILABLE);
#endif
    assert(body.used==0 && result[0]==0);
    assert(status==0 && bytes==0);
    puts("Cloud360 HTTPS POST fail-closed with missing CA: PASS");
    return 0;
}
