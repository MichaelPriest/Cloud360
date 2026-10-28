#include "../src/oauth.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static Cloud360OAuth flow;
static Cloud360OAuthDevice device(void)
{
    Cloud360OAuthDevice d={0};
    strcpy(d.user_code,"ABCD-EFGH");
    strcpy(d.verification_uri,"https://microsoft.com/devicelogin");
    strcpy(d.device_code,"private-internal-secret-value");
    d.expires_in_seconds=900;
    d.interval_seconds=5;
    return d;
}

int main(void)
{
    cloud360_oauth_init(&flow,false);
    assert(flow.status==CLOUD360_OAUTH_NOT_CONFIGURED);
    assert(!cloud360_oauth_begin(&flow));
    cloud360_oauth_init(&flow,true);
    assert(cloud360_oauth_begin(&flow));
    assert(flow.status==CLOUD360_OAUTH_REQUESTING);
    assert(!cloud360_oauth_begin(&flow));

    Cloud360OAuthDevice d=device();
    assert(cloud360_oauth_device(&flow,&d,1000));
    Cloud360OAuthView view;
    cloud360_oauth_view(&flow,1000,&view);
    assert(view.remaining_seconds==900 && view.interval_seconds==5);
    assert(!strcmp(view.user_code,"ABCD-EFGH"));
    assert(!strcmp(view.verification_uri,"https://microsoft.com/devicelogin"));
    assert(!cloud360_oauth_due(&flow,5999));
    assert(cloud360_oauth_due(&flow,6000));
    assert(!cloud360_oauth_due(&flow,6001));
    cloud360_oauth_reply(&flow,CLOUD360_OAUTH_REPLY_PENDING,6000);
    assert(!cloud360_oauth_due(&flow,10999));
    assert(cloud360_oauth_due(&flow,11000));
    cloud360_oauth_reply(&flow,CLOUD360_OAUTH_REPLY_SLOW_DOWN,11000);
    assert(flow.interval_ms==10000);
    assert(!cloud360_oauth_due(&flow,20999));
    assert(cloud360_oauth_due(&flow,21000));
    cloud360_oauth_reply(&flow,CLOUD360_OAUTH_REPLY_APPROVED,21000);
    assert(flow.status==CLOUD360_OAUTH_AUTHORIZED);
    assert(flow.device.device_code[0]==0);
    cloud360_oauth_view(&flow,21000,&view);
    assert(view.user_code[0]==0 && view.verification_uri[0]==0);
    assert(!cloud360_oauth_due(&flow,25000));

    cloud360_oauth_logout(&flow);
    assert(flow.status==CLOUD360_OAUTH_IDLE);
    assert(cloud360_oauth_begin(&flow));
    assert(cloud360_oauth_device(&flow,&d,UINT32_MAX-4000));
    assert(!cloud360_oauth_due(&flow,UINT32_MAX-1000));
    assert(cloud360_oauth_due(&flow,1000));
    cloud360_oauth_reply(&flow,CLOUD360_OAUTH_REPLY_DENIED,1000);
    assert(flow.status==CLOUD360_OAUTH_DECLINED && flow.device.device_code[0]==0);

    assert(cloud360_oauth_begin(&flow));
    d.interval_seconds=0;
    assert(cloud360_oauth_device(&flow,&d,100));
    assert(flow.interval_ms==5000);
    cloud360_oauth_tick(&flow,900100);
    assert(flow.status==CLOUD360_OAUTH_EXPIRED);
    assert(flow.device.device_code[0]==0);

    assert(cloud360_oauth_begin(&flow));
    d.interval_seconds=121;
    assert(!cloud360_oauth_device(&flow,&d,0));
    d.interval_seconds=5;
    strcpy(d.verification_uri,"http://microsoft.com/devicelogin");
    assert(!cloud360_oauth_device(&flow,&d,0));
    strcpy(d.verification_uri,"https://microsoft.com/devicelogin");
    d.expires_in_seconds=1;
    assert(!cloud360_oauth_device(&flow,&d,0));
    d.expires_in_seconds=900;
    assert(cloud360_oauth_device(&flow,&d,0));
    cloud360_oauth_cancel(&flow);
    assert(flow.status==CLOUD360_OAUTH_CANCELLED);
    assert(flow.device.device_code[0]==0);
    assert(!cloud360_oauth_due(&flow,5000));
    assert(cloud360_oauth_begin(&flow));
    assert(cloud360_oauth_device(&flow,&d,0));
    assert(cloud360_oauth_due(&flow,5000));
    cloud360_oauth_reply(&flow,CLOUD360_OAUTH_REPLY_EXPIRED,5000);
    assert(flow.status==CLOUD360_OAUTH_EXPIRED);
    puts("Cloud360 RFC8628 polling, slow_down, expiry, cancel, rollover: PASS");
    return 0;
}
