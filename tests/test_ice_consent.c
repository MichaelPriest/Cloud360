#include "../src/ice_consent.h"
#include <openssl/hmac.h>
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint32_t random_calls;
    uint32_t sends;
    bool fail_random;
    bool repeat_random;
    bool fail_send;
    Cloud360UdpPeer sent_to;
    uint8_t packet[CLOUD360_STUN_PACKET_MAX];
    size_t length;
} Fake;

static Cloud360IceConsent consent;
static Fake fake;
static const Cloud360UdpPeer server = {{192,0,2,11}, 53000};
static const Cloud360UdpPeer stranger = {{192,0,2,12}, 53000};
static const char *const password = "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";
static const Cloud360ConsentConfig config = {
    .nominated_remote = {{192,0,2,11}, 53000},
    .local_ufrag = "local123",
    .remote_ufrag = "remote456",
    .remote_password = "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890",
    .selected_pair_confirmed = true
};

static bool sha1(void *ctx,const uint8_t *key,size_t key_len,
                 const uint8_t *data,size_t len,uint8_t out[20])
{
    (void)ctx;
    if(key_len>INT_MAX)return false;
    unsigned n=0;
    return HMAC(EVP_sha1(),key,(int)key_len,data,len,out,&n) && n==20;
}
static bool random_bytes(void *ctx,uint8_t *data,size_t bytes)
{
    Fake *f=ctx;
    ++f->random_calls;
    if(f->fail_random)return false;
    for(size_t i=0;i<bytes;++i)
        data[i]=(uint8_t)(i+1+(f->repeat_random?0:f->random_calls));
    return true; /* For host tests ONLY. Production needs CSPRNG. */
}
static bool send_packet(void *ctx,const Cloud360UdpPeer *peer,
                        const uint8_t *packet,size_t length)
{
    Fake *f=ctx;
    if(f->fail_send)return false;
    assert(length<sizeof f->packet);
    f->sent_to=*peer;
    memcpy(f->packet,packet,length);
    f->length=length;
    ++f->sends;
    return true;
}
static void put16(uint8_t *p,uint16_t v)
{
    p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;
}
static void put32(uint8_t *p,uint32_t v)
{
    p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);
    p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;
}
static uint32_t crc(const uint8_t *p,size_t n)
{
    uint32_t x=UINT32_MAX;
    for(size_t i=0;i<n;++i){
        x^=p[i];
        for(unsigned j=0;j<8;++j)
            x=(x>>1)^(0xedb88320u&(0u-(x&1u)));
    }
    return ~x ^ 0x5354554eu;
}
static size_t reply(const uint8_t transaction_id[12],
                    uint8_t *out,size_t cap)
{
    assert(cap>=64);
    memset(out,0,cap);
    put16(out,0x0101);
    put32(out+4,0x2112a442);
    memcpy(out+8,transaction_id,12);
    static const uint8_t mapped[12]={
        0x00,0x20,0x00,0x08,0x00,0x01,0xa1,0x47,
        0xe1,0x12,0xa6,0x43
    };
    memcpy(out+20,mapped,12);
    put16(out+32,0x0008);put16(out+34,20);
    put16(out+2,36);
    uint8_t hmac[20];
    assert(sha1(NULL,(const uint8_t *)password,strlen(password),
                out,32,hmac));
    memcpy(out+36,hmac,20);
    put16(out+2,44);
    put16(out+56,0x8028);put16(out+58,4);
    put32(out+60,crc(out,56));
    return 64;
}
static void reset(void)
{
    cloud360_consent_init(&consent);
    memset(&fake,0,sizeof fake);
}
static void start_at(uint32_t now)
{
    assert(cloud360_consent_start(&consent,&config,
                                  sha1,random_bytes,send_packet,&fake,now));
}
static void test_validation_and_cryptographic_failures(void)
{
    reset();
    Cloud360ConsentConfig invalid=config;
    invalid.selected_pair_confirmed=false;
    assert(!cloud360_consent_start(&consent,&invalid,
                                   sha1,random_bytes,send_packet,&fake,0));
    assert(consent.state==CLOUD360_CONSENT_IDLE);
    invalid=config;
    invalid.nominated_remote.port=0;
    assert(!cloud360_consent_start(&consent,&invalid,
                                   sha1,random_bytes,send_packet,&fake,0));
    invalid=config;
    invalid.remote_password="short";
    assert(!cloud360_consent_start(&consent,&invalid,
                                   sha1,random_bytes,send_packet,&fake,0));
    assert(!cloud360_consent_start(&consent,&config,
                                   NULL,random_bytes,send_packet,&fake,0));
    assert(consent.state==CLOUD360_CONSENT_CRYPTO_UNAVAILABLE);
    assert(consent.remote_password[0]==0);
    assert(!cloud360_consent_can_send(&consent,0));
    reset();
    fake.fail_random=true;
    start_at(100);
    assert(!cloud360_consent_tick(&consent,100));
    assert(consent.state==CLOUD360_CONSENT_CRYPTO_UNAVAILABLE);
    assert(consent.remote_password[0]==0);

    reset();
    fake.fail_send=true;
    start_at(100);
    assert(!cloud360_consent_tick(&consent,100));
    assert(consent.state==CLOUD360_CONSENT_CRYPTO_UNAVAILABLE);
}
static void test_request_integrity_and_one_shot(void)
{
    reset();
    start_at(1000);
    assert(!cloud360_consent_can_send(&consent,1000));
    assert(cloud360_consent_tick(&consent,1000));
    assert(fake.sends==1 && fake.random_calls==1);
    assert(fake.sent_to.port==server.port);
    assert(!memcmp(fake.sent_to.ip,server.ip,4));
    assert(fake.packet[0]==0 && fake.packet[1]==1);
    /* RFC 7675 consent checks MUST NOT nominate or change ICE roles. */
    for(size_t at=20;at<fake.length;){
        assert(at+4<=fake.length);
        const unsigned type=(unsigned)fake.packet[at]*256+fake.packet[at+1];
        const unsigned len=(unsigned)fake.packet[at+2]*256+fake.packet[at+3];
        assert(type!=0x0025 && type!=0x0024 &&
               type!=0x8029 && type!=0x802a);
        at+=4u+((len+3u)&~3u);
    }
    uint8_t first_id[12];
    memcpy(first_id,fake.packet+8,12);
    uint8_t response[80];
    const size_t n=reply(first_id,response,sizeof response);
    const uint32_t gen=consent.generation;
    assert(!cloud360_consent_receive(&consent,gen,stranger,response,n,1100));
    assert(consent.rejected_responses==1);
    assert(!cloud360_consent_can_send(&consent,1100));
    response[40]^=1; /* corrupt HMAC */
    assert(!cloud360_consent_receive(&consent,gen,server,response,n,1101));
    response[40]^=1;
    assert(cloud360_consent_receive(&consent,gen,server,response,n,1102));
    assert(cloud360_consent_can_send(&consent,1102));
    assert(!cloud360_consent_receive(&consent,gen,server,response,n,1103));
    assert(consent.rejected_responses==3);

    Cloud360ConsentView view;
    cloud360_consent_view(&consent,1102,&view);
    assert(view.can_send_media && view.verified_responses==1);
    assert(!cloud360_consent_tick(&consent,4999)); /* minimum 4s */
    assert(fake.sends==1);
    assert(cloud360_consent_tick(&consent,7000)); /* <=6s randomized */
    assert(fake.sends==2);
    assert(memcmp(first_id,fake.packet+8,12)!=0);
    uint8_t newer_id[12];
    memcpy(newer_id,fake.packet+8,12);
    assert(!cloud360_consent_tick(&consent,7001));

    const size_t n2=reply(newer_id,response,sizeof response);
    assert(cloud360_consent_receive(&consent,gen,server,response,n2,7100));
    assert(consent.verified_responses==2);
    /* Replays from either transaction cannot refresh consent. */
    assert(!cloud360_consent_receive(&consent,gen,server,response,n2,7101));
    assert(!cloud360_consent_receive(&consent,gen,server,response,n,7102));
    assert(cloud360_consent_can_send(&consent,37100-1));
    assert(!cloud360_consent_can_send(&consent,37100));
    assert(!cloud360_consent_tick(&consent,37100));
    assert(consent.state==CLOUD360_CONSENT_EXPIRED);
    assert(consent.remote_password[0]==0);
    assert(!cloud360_consent_receive(&consent,gen,server,response,n2,37101));
    cloud360_consent_view(&consent,37101,&view);
    assert(!view.can_send_media && view.state==CLOUD360_CONSENT_EXPIRED);
}
static void test_no_initial_consent_and_rollover(void)
{
    reset();
    start_at(UINT32_MAX-1000);
    assert(cloud360_consent_tick(&consent,UINT32_MAX-1000));
    const uint32_t gen=consent.generation;
    uint8_t response[80];
    const size_t n=reply(fake.packet+8,response,sizeof response);
    assert(cloud360_consent_receive(&consent,gen,server,response,n,300));
    assert(cloud360_consent_can_send(&consent,300));
    assert(!cloud360_consent_can_send(&consent,30300));
    cloud360_consent_tick(&consent,30300);
    assert(consent.state==CLOUD360_CONSENT_EXPIRED);

    reset();
    start_at(0);
    assert(cloud360_consent_tick(&consent,0));
    const uint32_t old=consent.generation;
    const size_t late_len=reply(fake.packet+8,response,sizeof response);
    assert(!cloud360_consent_can_send(&consent,29999));
    assert(!cloud360_consent_receive(&consent,old,server,
                                     response,late_len,30000));
    assert(consent.state==CLOUD360_CONSENT_EXPIRED);
}
static void test_cancel_revoke_and_duplicate_randomness(void)
{
    reset();
    start_at(200);
    assert(cloud360_consent_tick(&consent,200));
    const uint32_t old=consent.generation;
    uint8_t response[80];
    const size_t len=reply(fake.packet+8,response,sizeof response);
    cloud360_consent_cancel(&consent);
    assert(consent.state==CLOUD360_CONSENT_CANCELLED);
    assert(consent.generation!=old);
    assert(!cloud360_consent_receive(&consent,old,server,response,len,201));
    assert(!cloud360_consent_can_send(&consent,201));

    reset();
    start_at(0);
    assert(cloud360_consent_tick(&consent,0));
    const uint32_t gen=consent.generation;
    assert(cloud360_consent_receive(&consent,gen,server,
                                    response,len,1));
    cloud360_consent_revoke(&consent);
    assert(consent.state==CLOUD360_CONSENT_REVOKED);
    assert(consent.remote_password[0]==0);
    assert(!cloud360_consent_can_send(&consent,1));

    reset();
    fake.repeat_random=true;
    start_at(0);
    assert(cloud360_consent_tick(&consent,0));
    assert(!cloud360_consent_tick(&consent,4000));
    assert(cloud360_consent_tick(&consent,6000)==false);
    assert(consent.state==CLOUD360_CONSENT_CRYPTO_UNAVAILABLE);
}
int main(void)
{
    test_validation_and_cryptographic_failures();
    test_request_integrity_and_one_shot();
    test_no_initial_consent_and_rollover();
    test_cancel_revoke_and_duplicate_randomness();
    puts("Cloud360 consent freshness, 30s expiry, replay, one-shot, origin: PASS");
    return 0;
}
