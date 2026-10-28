#include "../src/ice_agent.h"

#include <openssl/hmac.h>
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint8_t datagram[CLOUD360_STUN_PACKET_MAX];
    size_t size;
    Cloud360UdpPeer destination;
    uint32_t sent;
    uint32_t random_calls;
    bool fail_send;
    bool fail_random;
    bool zero_random;
} Harness;

static Harness harness;
static Cloud360IceAgent agent;
static const Cloud360UdpPeer first = {{192,0,2,21}, 50101};
static const Cloud360UdpPeer second = {{198,51,100,2}, 50102};
static const Cloud360UdpPeer attacker = {{203,0,113,44}, 50101};
static const Cloud360IceAgentConfig cfg = {
    .local_ufrag = "local123",
    .remote_ufrag = "remote456",
    .remote_pwd = "abcdefghijklmnopqrstuvwxyz1234567890",
    .candidate_priority = 1845494271u,
    .tie_breaker = UINT64_C(0x1122334455667788),
    .controlling = true
};

static bool sha1(void *context,const uint8_t *key,size_t key_len,
                 const uint8_t *data,size_t bytes,uint8_t digest[20])
{
    (void)context;
    if(key_len > INT_MAX)return false;
    unsigned n=0;
    return HMAC(EVP_sha1(),key,(int)key_len,data,bytes,digest,&n) && n==20;
}
static bool random_id(void *context,uint8_t *data,size_t size)
{
    Harness *h=context;
    ++h->random_calls;
    if(h->fail_random)return false;
    for(size_t i=0;i<size;++i)
        data[i]=h->zero_random?0:(uint8_t)(i+1+h->random_calls);
    return true; /* deterministic for TESTS only, never production */
}
static bool send_stub(void *context,const Cloud360UdpPeer *to,
                      const uint8_t *data,size_t bytes)
{
    Harness *h=context;
    if(h->fail_send)return false;
    assert(bytes >= 40 && bytes <= CLOUD360_STUN_PACKET_MAX);
    ++h->sent;
    h->destination=*to;
    h->size=bytes;
    memcpy(h->datagram,data,bytes);
    return true;
}
static void put16(uint8_t *p,uint16_t n)
{
    p[0]=(uint8_t)(n>>8);p[1]=(uint8_t)n;
}
static void put32(uint8_t *p,uint32_t n)
{
    p[0]=(uint8_t)(n>>24);p[1]=(uint8_t)(n>>16);
    p[2]=(uint8_t)(n>>8);p[3]=(uint8_t)n;
}
static uint32_t crc32_stun(const uint8_t *p,size_t bytes)
{
    uint32_t crc=UINT32_MAX;
    for(size_t i=0;i<bytes;++i){
        crc^=p[i];
        for(unsigned b=0;b<8;++b)
            crc=(crc>>1)^(0xedb88320u&(0u-(crc&1u)));
    }
    return ~crc ^ 0x5354554eu;
}
static size_t make_reply(const uint8_t tx[12],uint8_t *packet,size_t cap)
{
    assert(cap>=64);
    memset(packet,0,cap);
    put16(packet,0x0101);
    put16(packet+2,44);
    put32(packet+4,0x2112a442);
    memcpy(packet+8,tx,12);
    /* XOR-MAPPED-ADDRESS: 192.0.2.1 / port 32853 */
    const uint8_t map[12]={
        0x00,0x20,0x00,0x08,0x00,0x01,0xa1,0x47,
        0xe1,0x12,0xa6,0x43
    };
    memcpy(packet+20,map,sizeof map);
    put16(packet+32,0x0008);put16(packet+34,20);
    put16(packet+2,36);
    uint8_t digest[20];
    assert(sha1(NULL,(const uint8_t *)cfg.remote_pwd,
                strlen(cfg.remote_pwd),packet,32,digest));
    memcpy(packet+36,digest,20);
    put16(packet+2,44);
    put16(packet+56,0x8028);put16(packet+58,4);
    put32(packet+60,crc32_stun(packet,56));
    return 64;
}
static void prepare(void)
{
    memset(&harness,0,sizeof harness);
    cloud360_ice_agent_init(&agent);
}
static void add_and_start(void)
{
    assert(cloud360_ice_agent_add_pair(&agent,second,10));
    assert(cloud360_ice_agent_add_pair(&agent,first,100));
    assert(cloud360_ice_agent_start(
        &agent,&cfg,sha1,random_id,send_stub,&harness));
    assert(agent.generation);
}
static void test_pair_selection_and_retry(void)
{
    prepare();
    add_and_start();
    assert(agent.count==2 && agent.pairs[0].priority==100);
    const uint32_t generation=agent.generation;
    cloud360_ice_agent_tick(&agent,1000);
    assert(harness.sent==1);
    assert(harness.destination.port==first.port);
    const uint8_t tx[12]={0}; /* used only to check sent is nonzero */
    assert(memcmp(harness.datagram+8,tx,12)!=0);
    uint8_t first_id[12];
    memcpy(first_id,harness.datagram+8,12);
    cloud360_ice_agent_tick(&agent,1249);
    assert(harness.sent==1);
    cloud360_ice_agent_tick(&agent,1250);
    assert(harness.sent==2);
    assert(!memcmp(first_id,harness.datagram+8,12)); /* reuse on retry */
    cloud360_ice_agent_tick(&agent,1749);
    assert(harness.sent==2);
    cloud360_ice_agent_tick(&agent,1750);
    assert(harness.sent==3);
    cloud360_ice_agent_tick(&agent,2750);
    assert(harness.sent==4);
    cloud360_ice_agent_tick(&agent,4750);
    assert(harness.sent==5);
    cloud360_ice_agent_tick(&agent,8749);
    assert(harness.sent==5);
    cloud360_ice_agent_tick(&agent,8750);
    assert(harness.sent==6);
    assert(harness.destination.port==second.port);
    assert(agent.active_index==1);
    assert(agent.pairs[0].state==CLOUD360_ICE_PAIR_FAILED);
    assert(memcmp(first_id,harness.datagram+8,12)!=0);

    uint8_t reply[100];
    size_t n=make_reply(first_id,reply,sizeof reply);
    assert(!cloud360_ice_agent_receive(&agent,generation,
                                       first,reply,n,8751));
    /* source must match second pair and transaction ID. */
    uint8_t second_id[12];
    memcpy(second_id,harness.datagram+8,12);
    n=make_reply(second_id,reply,sizeof reply);
    assert(!cloud360_ice_agent_receive(&agent,generation,
                                       attacker,reply,n,8752));
    Cloud360IceAgentView view;
    cloud360_ice_agent_view(&agent,&view);
    assert(view.invalid_responses==2 && view.sends_total==6);
    assert(view.state==CLOUD360_ICE_AGENT_CHECKING);

    reply[48]^=1; /* fake HMAC */
    assert(!cloud360_ice_agent_receive(&agent,generation,
                                       second,reply,n,8753));
    reply[48]^=1;
    assert(cloud360_ice_agent_receive(&agent,generation,
                                      second,reply,n,8754));
    cloud360_ice_agent_view(&agent,&view);
    assert(view.state==CLOUD360_ICE_AGENT_VERIFIED_NOT_NOMINATED);
    assert(view.has_authenticated_response);
    assert(view.active_pair==1);
    assert(view.invalid_responses==3);
    assert(view.generation!=generation);
    assert(agent.remote_password[0]==0);
    assert(agent.pairs[1].transaction_id[0]==0);
    assert(!cloud360_ice_agent_receive(&agent,generation,
                                       second,reply,n,8755));
    cloud360_ice_agent_tick(&agent,20000);
    assert(harness.sent==6);
}
static void test_retransmit_exhaust_and_rollover(void)
{
    prepare();
    assert(cloud360_ice_agent_add_pair(&agent,first,10));
    assert(cloud360_ice_agent_start(
        &agent,&cfg,sha1,random_id,send_stub,&harness));
    const uint32_t gen=agent.generation;
    const uint32_t start=UINT32_MAX-99;
    cloud360_ice_agent_tick(&agent,start);
    assert(harness.sent==1);
    cloud360_ice_agent_tick(&agent,149);
    assert(harness.sent==1);
    cloud360_ice_agent_tick(&agent,150);
    assert(harness.sent==2);
    cloud360_ice_agent_tick(&agent,650);
    assert(harness.sent==3);
    cloud360_ice_agent_tick(&agent,1650);
    assert(harness.sent==4);
    cloud360_ice_agent_tick(&agent,3650);
    assert(harness.sent==5);
    cloud360_ice_agent_tick(&agent,7650);
    assert(agent.state==CLOUD360_ICE_AGENT_EXHAUSTED);
    assert(agent.remote_password[0]==0);
    assert(agent.generation!=gen);
    Cloud360IceAgentView view;
    cloud360_ice_agent_view(&agent,&view);
    assert(!view.has_authenticated_response && view.sends_total==5);
}
static void test_cancel_source_timeout(void)
{
    prepare();
    assert(cloud360_ice_agent_add_pair(&agent,first,1));
    assert(cloud360_ice_agent_start(
        &agent,&cfg,sha1,random_id,send_stub,&harness));
    cloud360_ice_agent_tick(&agent,10);
    const uint32_t gen=agent.generation;
    uint8_t reply[100];
    const size_t n=make_reply(harness.datagram+8,reply,sizeof reply);
    assert(!cloud360_ice_agent_receive(&agent,gen,first,reply,n,
                                       12010)); /* expired */
    cloud360_ice_agent_tick(&agent,12010);
    assert(agent.state==CLOUD360_ICE_AGENT_EXHAUSTED);

    prepare();
    add_and_start();
    cloud360_ice_agent_tick(&agent,1);
    const uint32_t gen2=agent.generation;
    cloud360_ice_agent_cancel(&agent);
    assert(agent.state==CLOUD360_ICE_AGENT_CANCELLED);
    assert(agent.remote_password[0]==0);
    assert(agent.generation!=gen2);
    assert(!cloud360_ice_agent_receive(&agent,gen2,first,reply,n,2));
    cloud360_ice_agent_tick(&agent,1000);
    assert(harness.sent==1);
}
static void test_fail_closed_and_limits(void)
{
    prepare();
    assert(!cloud360_ice_agent_add_pair(&agent,
            (Cloud360UdpPeer){{0,0,0,0},1234},42));
    assert(!cloud360_ice_agent_add_pair(&agent,
            (Cloud360UdpPeer){{224,0,0,1},1234},42));
    assert(!cloud360_ice_agent_add_pair(&agent,
            (Cloud360UdpPeer){{127,0,0,1},0},42));
    assert(!cloud360_ice_agent_add_pair(&agent,first,0));
    assert(cloud360_ice_agent_add_pair(&agent,first,1));
    assert(!cloud360_ice_agent_add_pair(&agent,first,3));
    for(uint32_t i=1;i<CLOUD360_ICE_AGENT_PAIRS_MAX;++i){
        Cloud360UdpPeer ip={{192,0,2,(uint8_t)(i+21)},(uint16_t)(50000+i)};
        assert(cloud360_ice_agent_add_pair(&agent,ip,i));
    }
    assert(!cloud360_ice_agent_add_pair(&agent,second,500));

    assert(!cloud360_ice_agent_start(
        &agent,&cfg,NULL,random_id,send_stub,&harness));
    assert(agent.state==CLOUD360_ICE_AGENT_CRYPTO_UNAVAILABLE);

    prepare();
    assert(cloud360_ice_agent_add_pair(&agent,first,10));
    Cloud360IceAgentConfig bad=cfg;
    bad.remote_pwd="oops";
    assert(!cloud360_ice_agent_start(&agent,&bad,
                                     sha1,random_id,send_stub,&harness));
    bad=cfg;
    bad.local_ufrag="abcd:injection";
    assert(!cloud360_ice_agent_start(&agent,&bad,
                                     sha1,random_id,send_stub,&harness));
    assert(cloud360_ice_agent_start(
        &agent,&cfg,sha1,random_id,send_stub,&harness));
    assert(!cloud360_ice_agent_start(
        &agent,&cfg,sha1,random_id,send_stub,&harness));
    harness.fail_send=true;
    cloud360_ice_agent_tick(&agent,42);
    assert(agent.state==CLOUD360_ICE_AGENT_EXHAUSTED);
    assert(harness.sent==0);

    prepare();
    assert(cloud360_ice_agent_add_pair(&agent,first,1));
    assert(cloud360_ice_agent_start(
        &agent,&cfg,sha1,random_id,send_stub,&harness));
    harness.fail_random=true;
    cloud360_ice_agent_tick(&agent,100);
    assert(agent.state==CLOUD360_ICE_AGENT_EXHAUSTED);

    prepare();
    assert(cloud360_ice_agent_add_pair(&agent,first,1));
    assert(cloud360_ice_agent_start(
        &agent,&cfg,sha1,random_id,send_stub,&harness));
    harness.zero_random=true;
    cloud360_ice_agent_tick(&agent,100);
    assert(agent.state==CLOUD360_ICE_AGENT_EXHAUSTED);
}
int main(void)
{
    test_pair_selection_and_retry();
    test_retransmit_exhaust_and_rollover();
    test_cancel_source_timeout();
    test_fail_closed_and_limits();
    puts("Cloud360 ICE candidate pairs, retries, HMAC, peer origin, cancel: PASS");
    return 0;
}
