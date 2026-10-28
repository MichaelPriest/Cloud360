#include "xbl.h"
#include <limits.h>
#include <string.h>

enum { MAX_DEPTH=10 };
typedef struct { const char *s; size_t n,p; } Read;

void cloud360_xbl_wipe(void *data,size_t n)
{
    volatile unsigned char *p=data;
    while(n--)*p++=0;
}

static bool app(char *out,size_t cap,size_t *at,const char *s)
{
    size_t n=strlen(s);
    if(*at>=cap || n>=cap-*at)return false;
    memcpy(out+*at,s,n+1);
    *at+=n;
    return true;
}
static bool quoted_token(char *out,size_t cap,size_t *at,
                         const char *token,bool prefix)
{
    if(!token||!*token||!app(out,cap,at,"\""))return false;
    if(prefix&&!app(out,cap,at,"d="))return false;
    for(size_t i=0;token[i];++i){
        unsigned char c=(unsigned char)token[i];
        if(c<33||c>126)return false;
        if((c=='"'||c=='\\')&&!app(out,cap,at,"\\"))return false;
        char byte[2]={(char)c,0};
        if(!app(out,cap,at,byte))return false;
    }
    return app(out,cap,at,"\"");
}
bool cloud360_xbl_user_request(const char *token,char *out,size_t capacity)
{
    if(!out||!capacity)return false;
    out[0]=0;
    if(!token || !*token || strlen(token)>=CLOUD360_XBL_TOKEN_MAX)
        return false;
    size_t used=0;
    return app(out,capacity,&used,
       "{\"RelyingParty\":\"http://auth.xboxlive.com\","
       "\"TokenType\":\"JWT\",\"Properties\":{\"AuthMethod\":\"RPS\","
       "\"SiteName\":\"user.auth.xboxlive.com\",\"RpsTicket\":")
       && quoted_token(out,capacity,&used,token,true)
       && app(out,capacity,&used,"}}");
}
bool cloud360_xbl_xsts_request(const char *token,char *out,size_t capacity)
{
    if(!out||!capacity)return false;
    out[0]=0;
    if(!token || !*token || strlen(token)>=CLOUD360_XBL_TOKEN_MAX)
        return false;
    size_t used=0;
    return app(out,capacity,&used,
       "{\"RelyingParty\":\"http://xboxlive.com\",\"TokenType\":\"JWT\","
       "\"Properties\":{\"SandboxId\":\"RETAIL\",\"UserTokens\":[")
       && quoted_token(out,capacity,&used,token,false)
       && app(out,capacity,&used,"]}}");
}
static void ws(Read *r)
{
    while(r->p<r->n){
        const char c=r->s[r->p];
        if(c!=' '&&c!='\t'&&c!='\n'&&c!='\r')break;
        ++r->p;
    }
}
static bool eat(Read *r,char ch)
{
    ws(r);
    if(r->p<r->n&&r->s[r->p]==ch){++r->p;return true;}
    return false;
}
static bool str(Read *r,char *dst,size_t cap)
{
    if(!eat(r,'"'))return false;
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
                case 'u':{
                    if(r->n-r->p<4)return false;
                    unsigned code=0;
                    for(int i=0;i<4;i++){
                        unsigned char h=(unsigned char)r->s[r->p++];
                        unsigned value;
                        if(h>='0'&&h<='9')value=h-'0';
                        else if(h>='a'&&h<='f')value=h-'a'+10;
                        else if(h>='A'&&h<='F')value=h-'A'+10;
                        else return false;
                        code=(code<<4)|value;
                    }
                    if(code>=0xD800&&code<=0xDFFF)return false;
                    if(!dst)continue;
                    if(code<33||code>126)return false;
                    c=(unsigned char)code;
                    break;
                }
                default:return false;
            }
        }
        if(dst){
            if(c<33||c>126||used+1>=cap)return false;
            dst[used++]=(char)c;
        }
    }
    return false;
}
static bool literal(Read *r,const char *s)
{
    size_t n=strlen(s);
    if(r->n-r->p<n||memcmp(r->s+r->p,s,n))return false;
    r->p+=n;return true;
}
static bool number(Read *r,uint32_t *out)
{
    ws(r);size_t before=r->p;
    uint32_t result=0;
    if(r->p==r->n || r->s[r->p]<'0'||r->s[r->p]>'9')return false;
    if(r->s[r->p]=='0')++r->p;
    else while(r->p<r->n&&r->s[r->p]>='0'&&r->s[r->p]<='9'){
        unsigned c=(unsigned)(r->s[r->p++]-'0');
        if(result>(UINT32_MAX-c)/10)return false;
        result=result*10+c;
    }
    if(r->p==before)return false;
    *out=result;return true;
}
static bool skip(Read *r,unsigned depth)
{
    if(depth>MAX_DEPTH)return false;
    ws(r);if(r->p>=r->n)return false;
    char ch=r->s[r->p];
    if(ch=='"')return str(r,NULL,0);
    if(ch=='['||ch=='{'){
        ++r->p;const char end=ch=='['?']':'}';
        if(eat(r,end))return true;
        do{
            if(ch=='{'&&(!str(r,NULL,0)||!eat(r,':')))return false;
            if(!skip(r,depth+1))return false;
            if(eat(r,end))return true;
        }while(eat(r,','));
        return false;
    }
    if(ch=='t')return literal(r,"true");
    if(ch=='f')return literal(r,"false");
    if(ch=='n')return literal(r,"null");
    if(ch=='-'||(ch>='0'&&ch<='9')){
        if(ch=='-')++r->p;
        uint32_t val=0;if(!number(r,&val))return false;
        if(r->p<r->n&&r->s[r->p]=='.'){
            ++r->p;size_t n=r->p;
            while(r->p<r->n&&r->s[r->p]>='0'&&r->s[r->p]<='9')++r->p;
            if(n==r->p)return false;
        }
        if(r->p<r->n&&(r->s[r->p]=='e'||r->s[r->p]=='E')){
            ++r->p;
            if(r->p<r->n&&(r->s[r->p]=='+'||r->s[r->p]=='-'))++r->p;
            size_t n=r->p;
            while(r->p<r->n&&r->s[r->p]>='0'&&r->s[r->p]<='9')++r->p;
            if(n==r->p)return false;
        }
        return true;
    }
    return false;
}
static bool xui(Read *r,char *hash,size_t cap)
{
    if(!eat(r,'['))return false;
    if(eat(r,']'))return true;
    if(!eat(r,'{'))return false;
    bool seen=false;
    if(!eat(r,'}')){
        do{
            char key[48]={0};
            if(!str(r,key,sizeof key)||!eat(r,':'))return false;
            if(!strcmp(key,"uhs")){
                if(seen||!str(r,hash,cap))return false;
                seen=true;
            }else if(!skip(r,2))return false;
            if(eat(r,'}'))break;
            if(!eat(r,','))return false;
        }while(true);
    }
    while(eat(r,','))if(!skip(r,2))return false;
    return eat(r,']');
}
static bool claims(Read *r,char *hash,size_t cap)
{
    if(!eat(r,'{'))return false;
    if(eat(r,'}'))return true;
    bool seen=false;
    do{
        char key[48]={0};
        if(!str(r,key,sizeof key)||!eat(r,':'))return false;
        if(!strcmp(key,"xui")){
            if(seen||!xui(r,hash,cap))return false;
            seen=true;
        }else if(!skip(r,1))return false;
        if(eat(r,'}'))return true;
    }while(eat(r,','));
    return false;
}
bool cloud360_xbl_parse_reply(const char *json,size_t bytes,Cloud360XblReply *out)
{
    if(!json||!out||!bytes||bytes>16384)return false;
    Cloud360XblReply reply={0};
    Read r={json,bytes,0};
    if(!eat(&r,'{')||eat(&r,'}'))return false;
    bool token=false,display=false,error=false;
    do{
        char key[48]={0};
        if(!str(&r,key,sizeof key)||!eat(&r,':'))return false;
        if(!strcmp(key,"Token")){
            if(token||!str(&r,reply.token,sizeof reply.token))return false;
            token=true;
        }else if(!strcmp(key,"DisplayClaims")){
            if(display||!claims(&r,reply.user_hash,sizeof reply.user_hash))
                return false;
            display=true;
        }else if(!strcmp(key,"XErr")){
            if(error||!number(&r,&reply.error_code))return false;
            reply.has_error=true;
            error=true;
        }else if(!skip(&r,0))return false;
        if(eat(&r,'}'))break;
        if(!eat(&r,','))return false;
    }while(true);
    ws(&r);
    if(r.p!=r.n)return false;
    if(!(reply.has_error&&reply.error_code)&&
       (!token||!display||!reply.token[0]||!reply.user_hash[0]))
        return false;
    *out=reply;
    return true;
}
const char *cloud360_xbl_status_text(Cloud360XblStatus status)
{
    switch(status){
        case CLOUD360_XBL_IDLE:return "XBOX LIVE NAO AUTENTICADO";
        case CLOUD360_XBL_REQUEST_USER:return "SOLICITANDO TOKEN XBOX...";
        case CLOUD360_XBL_REQUEST_XSTS:return "SOLICITANDO TOKEN XSTS...";
        case CLOUD360_XBL_AUTHORIZED:return "XBOX LIVE/XSTS AUTORIZADO";
        case CLOUD360_XBL_REJECTED:return "ACESSO XBOX LIVE RECUSADO";
        case CLOUD360_XBL_UNAVAILABLE:return "HTTPS XBOX INDISPONIVEL";
        case CLOUD360_XBL_BAD_RESPONSE:return "RESPOSTA XBOX INVALIDA";
        case CLOUD360_XBL_FAILED:return "FALHA DE AUTENTICACAO XBOX";
        case CLOUD360_XBL_CANCELLED:return "SESSAO XBOX CANCELADA";
        default:return "ESTADO DESCONHECIDO";
    }
}
const char *cloud360_xbl_error_text(uint32_t code)
{
    switch(code){
        case 2148916233u:return "PERFIL XBOX LIVE INEXISTENTE";
        case 2148916235u:return "REGIAO NAO SUPORTADA";
        case 2148916238u:return "CONTA EXIGE PERMISSAO FAMILIAR";
        default:return "ERRO XSTS DESCONHECIDO";
    }
}
