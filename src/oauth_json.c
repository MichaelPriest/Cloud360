#include "oauth_json.h"
#include <stdint.h>
#include <string.h>
#include <limits.h>

enum { MAX_JSON_BYTES = 8192, MAX_NESTING = 8 };
typedef struct {
    const char *s;
    size_t n;
    size_t p;
} Reader;

static void space(Reader *r)
{
    while (r->p < r->n) {
        char c = r->s[r->p];
        if (c != ' ' && c != '\r' && c != '\n' && c != '\t') break;
        ++r->p;
    }
}
static bool take(Reader *r, char c)
{
    space(r);
    if (r->p < r->n && r->s[r->p] == c) { ++r->p; return true; }
    return false;
}
static bool hex4(Reader *r, unsigned *out)
{
    if (r->n-r->p < 4) return false;
    unsigned value = 0;
    for (unsigned i=0;i<4;++i) {
        unsigned char c=(unsigned char)r->s[r->p++];
        unsigned n;
        if(c>='0'&&c<='9')n=c-'0';
        else if(c>='a'&&c<='f')n=c-'a'+10;
        else if(c>='A'&&c<='F')n=c-'A'+10;
        else return false;
        value=value*16+n;
    }
    *out=value;return true;
}
/* Extract only ASCII text (tokens, codes, URIs). For ignored fields,
 * the reader accepts ordinary UTF-8 without copying. */
static bool quoted(Reader *r, char *dst, size_t cap)
{
    if (!take(r,'"')) return false;
    size_t used=0;
    while(r->p<r->n){
        unsigned char c=(unsigned char)r->s[r->p++];
        if(c=='"'){if(dst)dst[used]=0;return true;}
        if(c<32)return false;
        if(c=='\\'){
            if(r->p>=r->n)return false;
            char e=r->s[r->p++];
            switch(e){
                case '"':c='"';break;
                case '\\':c='\\';break;
                case '/':c='/';break;
                case 'b':c='\b';break;
                case 'f':c='\f';break;
                case 'n':c='\n';break;
                case 'r':c='\r';break;
                case 't':c='\t';break;
                case 'u': {
                    unsigned u=0;
                    if(!hex4(r,&u))return false;
                    if(u>=0xD800 && u<=0xDFFF)return false;
                    if(u>127){
                        if(dst)return false;
                        continue;
                    }
                    c=(unsigned char)u;
                    break;
                }
                default:return false;
            }
        }
        if(dst){
            if(c<33 || c>126 || used+1>=cap)return false;
            dst[used++]=(char)c;
        }
    }
    return false;
}
static bool skip(Reader *r,unsigned depth);
static bool skip_literal(Reader *r,const char *value)
{
    size_t n=strlen(value);
    if(r->n-r->p<n||memcmp(r->s+r->p,value,n))return false;
    r->p+=n;return true;
}
static bool skip_number(Reader *r)
{
    size_t start=r->p;
    if(r->p<r->n&&r->s[r->p]=='-')++r->p;
    if(r->p>=r->n)return false;
    if(r->s[r->p]=='0')++r->p;
    else{
        if(r->s[r->p]<'1'||r->s[r->p]>'9')return false;
        while(r->p<r->n&&r->s[r->p]>='0'&&r->s[r->p]<='9')++r->p;
    }
    if(r->p<r->n&&r->s[r->p]=='.'){
        ++r->p;size_t d=r->p;
        while(r->p<r->n&&r->s[r->p]>='0'&&r->s[r->p]<='9')++r->p;
        if(r->p==d)return false;
    }
    if(r->p<r->n&&(r->s[r->p]=='e'||r->s[r->p]=='E')){
        ++r->p;
        if(r->p<r->n&&(r->s[r->p]=='+'||r->s[r->p]=='-'))++r->p;
        size_t d=r->p;
        while(r->p<r->n&&r->s[r->p]>='0'&&r->s[r->p]<='9')++r->p;
        if(r->p==d)return false;
    }
    return r->p>start;
}
static bool skip(Reader *r,unsigned depth)
{
    if(depth>MAX_NESTING)return false;
    space(r);if(r->p>=r->n)return false;
    char c=r->s[r->p];
    if(c=='"')return quoted(r,NULL,0);
    if(c=='{'||c=='['){
        ++r->p;
        const char ending=c=='{'?'}':']';
        if(take(r,ending))return true;
        do{
            if(c=='{'&&(!quoted(r,NULL,0)||!take(r,':')))return false;
            if(!skip(r,depth+1))return false;
            if(take(r,ending))return true;
        }while(take(r,','));
        return false;
    }
    if(c=='n')return skip_literal(r,"null");
    if(c=='t')return skip_literal(r,"true");
    if(c=='f')return skip_literal(r,"false");
    return skip_number(r);
}
static bool unsigned_value(Reader *r,uint32_t *out)
{
    space(r);size_t start=r->p;
    uint32_t value=0;
    while(r->p<r->n){
        unsigned char c=(unsigned char)r->s[r->p];
        if(c<'0'||c>'9')break;
        if((uint64_t)value*10+(c-'0')>UINT32_MAX)return false;
        value=value*10+(c-'0');
        ++r->p;
    }
    if(r->p==start)return false;
    *out=value;return true;
}
static bool key_once(unsigned *flags,unsigned mask)
{
    if((*flags&mask)!=0)return false;
    *flags|=mask;return true;
}

bool cloud360_oauth_parse_device_json(const char *json,size_t len,
                                      Cloud360OAuthDevice *out)
{
    if(!json||!out||!len||len>MAX_JSON_BYTES)return false;
    Cloud360OAuthDevice d={0}; Reader r={json,len,0};
    unsigned flags=0;
    if(!take(&r,'{')||take(&r,'}'))return false;
    do{
        char k[48]={0};
        if(!quoted(&r,k,sizeof k)||!take(&r,':'))return false;
        if(!strcmp(k,"device_code")){
            if(!key_once(&flags,1)||!quoted(&r,d.device_code,sizeof d.device_code))return false;
        }else if(!strcmp(k,"user_code")){
            if(!key_once(&flags,2)||!quoted(&r,d.user_code,sizeof d.user_code))return false;
        }else if(!strcmp(k,"verification_uri")){
            if(!key_once(&flags,4)||!quoted(&r,d.verification_uri,sizeof d.verification_uri))return false;
        }else if(!strcmp(k,"expires_in")){
            if(!key_once(&flags,8)||!unsigned_value(&r,&d.expires_in_seconds))return false;
        }else if(!strcmp(k,"interval")){
            if(!key_once(&flags,16)||!unsigned_value(&r,&d.interval_seconds))return false;
        }else if(!skip(&r,1))return false;
        if(take(&r,'}'))break;
        if(!take(&r,','))return false;
    }while(true);
    space(&r);
    if(r.p!=r.n||(flags&15)!=15)return false;
    *out=d;return true;
}

bool cloud360_oauth_parse_token_json(const char *json,size_t len,
                                     Cloud360OAuthTokenResponse *out)
{
    if(!json||!out||!len||len>MAX_JSON_BYTES)return false;
    Cloud360OAuthTokenResponse t={0};
    char error[64]={0},type[24]={0};
    unsigned flags=0;
    Reader r={json,len,0};
    if(!take(&r,'{')||take(&r,'}'))return false;
    do{
        char k[48]={0};
        if(!quoted(&r,k,sizeof k)||!take(&r,':'))return false;
        if(!strcmp(k,"error")){
            if(!key_once(&flags,1)||!quoted(&r,error,sizeof error))return false;
        }else if(!strcmp(k,"access_token")){
            if(!key_once(&flags,2)||!quoted(&r,t.access_token,sizeof t.access_token))return false;
        }else if(!strcmp(k,"refresh_token")){
            if(!key_once(&flags,4)||!quoted(&r,t.refresh_token,sizeof t.refresh_token))return false;
        }else if(!strcmp(k,"token_type")){
            if(!key_once(&flags,8)||!quoted(&r,type,sizeof type))return false;
        }else if(!skip(&r,1))return false;
        if(take(&r,'}'))break;
        if(!take(&r,','))return false;
    }while(true);
    space(&r);if(r.p!=r.n)return false;
    if(flags&1){
        if(!strcmp(error,"authorization_pending"))t.reply=CLOUD360_OAUTH_REPLY_PENDING;
        else if(!strcmp(error,"slow_down"))t.reply=CLOUD360_OAUTH_REPLY_SLOW_DOWN;
        else if(!strcmp(error,"authorization_declined")||
                !strcmp(error,"access_denied"))t.reply=CLOUD360_OAUTH_REPLY_DENIED;
        else if(!strcmp(error,"expired_token"))t.reply=CLOUD360_OAUTH_REPLY_EXPIRED;
        else t.reply=CLOUD360_OAUTH_REPLY_ERROR;
    }else{
        if(!(flags&2)||!(flags&8)||strcmp(type,"Bearer")||!t.access_token[0])
            return false;
        t.reply=CLOUD360_OAUTH_REPLY_APPROVED;
    }
    *out=t;return true;
}
