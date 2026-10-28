#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../src/https.h"

int main(void)
{
    Cloud360HttpsRequest request = {
        .host = "example.com",
        .path = "/",
        .ca_file = "game:/missing-ca-file.pem"
    };
    long status = 99;
    size_t bytes = 99;
    int error = 0;

    Cloud360HttpsStatus got = cloud360_https_curl_fetch(
        NULL, &request, &status, &bytes, &error);

#ifdef CLOUD360_WITH_CURL
    /* Missing CA blocks the attempt BEFORE opening any HTTPS connection.
       This test never uses the network. */
    assert(got == CLOUD360_HTTPS_CA_MISSING);
    assert(status == 0 && bytes == 0 && error == 0);
#else
    assert(got == CLOUD360_HTTPS_UNAVAILABLE);
    assert(status == 0 && bytes == 0 && error == ENOSYS);
#endif

    strcpy(request.host, "https://example.com");
    got = cloud360_https_curl_fetch(NULL, &request, &status, &bytes, &error);
    assert(got == CLOUD360_HTTPS_BAD_REQUEST);
    puts("Cloud360 curl backend fails closed without verified TLS: PASS");
    return 0;
}
