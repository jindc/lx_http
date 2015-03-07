#ifndef LX_HTTP_H
#define LX_HTTP_H
/*
request-line
headers
<blank line>
body

request-line
    method uri protocal
*/
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include "lx_list.h"
#include "lx_buffer.h"
#include "lx_string.h"
#include "lx_http_error.h"
#include "lx_types.h"

#define findarr_by_key(array, key) ((array)[key] )

extern int findarr_by_value( char ** array,char * value);

typedef enum h_method_t h_method_t;
enum h_method_t
{
    M_GET = 0,
    M_POST,
    M_HEAD
};
extern char *h_mtod_str[] ;

typedef enum h_protocal_t h_protocal_t;
enum h_protocal_t
{
    P_HTTP_0_X = 0,
    P_HTTP_1_0,
    P_HTTP_1_1
};
extern char * h_prot_str[] ;

extern int get_http_prot(char * buff,int len);


#define HTTP_HEADER_SEP_STR ": "
#define HTTP_NEW_LINE_STR "\r\n"

typedef enum h_resp_code_t h_resp_code_t;
enum h_resp_code_t
{
    RESP_OK = 200,
    RESP_REDIRECT = 300,
    RESP_CLIENT_ERR = 400,
    RESP_SERVER_ERR = 500
};
extern char * http_resp_str[];

typedef enum h_type_t h_type_t;
enum h_type_t
{
    T_REQ = 0,
    T_RESP
};

typedef enum h_stage_t h_stage_t;
enum h_stage_t
{
    STAGE_START,
    STAGE_LISTENING,
    STAGE_REQ_HEAD,
    STAGE_REQ_BODY,
    STAGE_RESP_HEAD,
    STAGE_RESP_BODY,
    STAGE_DONE
};
extern char * http_stage_str[];

typedef enum h_stat_t h_stat_t;
enum h_stat_t
{
    S_NONE = 0,

    S_REQ_METHOD,
    S_REQ_URI,
    S_REQ_PROTOCAL,
    
    S_RESP_PROTOCAL,
    S_RESP_CODE,
    S_RESP_STR,
    
    S_HEADER_KEY,
    S_HEADER_VALUE,

    S_BODY,

    S_HEADER_SEP,
    S_ITEM_SPACE,
    S_NEW_LINE
};

/*
header :two type,all '\0' end
type 1(after parser):lx_ebase_offset
	buff = info->base + lx_buffer.offset
type 2(after modify) lx_base
	buff = lx_buffer.base
*/
typedef struct h_head_info h_head_info;
struct h_head_info
{
    h_type_t type;
    char * base;

    lx_buffer mtod_str;
    h_method_t mtod;

    lx_buffer uri;

    char* uri_base;
    lx_buffer path;
    lx_kvlist_node * params; 

    lx_buffer prot_str;
    h_protocal_t prot;

    h_resp_code_t resp_code;
    lx_buffer code_str;
    
    lx_buffer header_key;
    lx_buffer header_value;
    lx_kvlist_node * headers;
};

typedef struct h_parser_ctx h_parser_ctx;
struct h_parser_ctx
{
    h_head_info info;

    h_type_t type;

    h_stat_t last_stat;
    h_stat_t cur_stat;
    h_stat_t next_stat;
    
    lx_buffer orig_buff;
    lx_buffer uri_buff;
    lx_buffer temp_buff;

    char spliter_path;
    char spliter_param;
    char spliter_pkv;

    void * (*hmalloc)(size_t);
    void   (*hfree)  (void *);
    int    (*hextend)(lx_buffer *);
};

#define http_get_respstr( code) find_by_int( http_resp_str,(code))
extern int http_set_prop1(h_parser_ctx *pctx,int intval, char ** strarr,void * key, lx_buffer * str);
extern int http_set_prop2(h_parser_ctx *pctx, lx_buffer * buff, char * nval);
extern int http_set_prop3(h_parser_ctx * pctx,lx_kvlist_node ** phead_node ,char * key, char * value,lx_bool_t is_header);

#define http_get_mtod(pinfo) ((pinfo)->mtod_str.base ?(pinfo)->mtod_str.base:(pinfo)->base +(pinfo)->mtod_str.offset)  
#define http_set_mtod(pctx, mtodval) \
    http_set_prop1(pctx,mtodval,h_mtod_str,& (pctx)->info.mtod,&(pctx)->info.mtod_str) 

#define http_get_uri(pinfo) ((pinfo)->uri.base ?(pinfo)->uri.base:(pinfo)->base +(pinfo)->uri.offset)  
#define http_set_uri(pctx,nval) http_set_prop2( (pctx),&(pctx)->info.uri,nval)

#define http_get_path(pinfo) ((pinfo)->path.base ?(pinfo)->path.base:(pinfo)->uri_base +(pinfo)->path.offset)  

#define http_param_begin(pinfo,pnode) ( (pnode) = (pinfo)->params )
#define http_param_next(pinfo,pnode) ( (pnode) =(lx_kvlist_node *) ( pnode)->list.next )
#define http_get_param( info,  key) (http_find_node_value( (info),(key),(info)->params, LX_FALSE));    
#define http_get_parambuff(pinfo,pbuff) \
    ((pbuff)->base == NULL ? ( (pinfo)->uri_base +(pbuff)->offset ):( (pbuff)->base ))

#define http_get_prot(pinfo) ((pinfo)->prot_str.base ?(pinfo)->prot_str.base:(pinfo)->base +(pinfo)->prot_str.offset)  
#define http_set_prot(pctx,protv) \
    http_set_prop1(pctx,protv,h_prot_str,& (pctx)->info.prot,&(pctx)->info.prot_str) 

#define http_get_code(pinfo) ( (pinfo)->resp_code)
#define http_get_codestr(pinfo) ((pinfo)->code_str.base ?(pinfo)->code_str.base:(pinfo)->base +(pinfo)->code_str.offset)  
extern int http_set_rcode(h_parser_ctx * pctx,h_resp_code_t code, char * code_str);

#define http_get_header( info,key)  (http_find_node_value( (info),(key),(info)->headers,LX_TRUE)) 
#define http_set_header(pctx,key,value)   \
    http_set_prop3(pctx,&(pctx)->info.headers,key,value,LX_TRUE)
#define http_header_begin(pinfo,pnode) ( (pnode) = (pinfo)->headers )
#define http_header_next(pinfo,pnode) ( (pnode) =(lx_kvlist_node *) ( pnode)->list.next )
#define http_get_buffval(pinfo,pbuff) \
    ((pbuff)->base == NULL ? ( (pinfo)->base +(pbuff)->offset ):( (pbuff)->base ))

extern int http_get_contlen(h_head_info *info);

extern lx_kvlist_node *http_find_node(h_head_info * info,char * key,lx_kvlist_node * head_node,lx_bool_t is_header);
extern char * http_find_node_value(h_head_info *info,char * key,lx_kvlist_node * head_node,lx_bool_t is_header);

extern int http_seri_head(h_head_info *pinfo,h_type_t type, char * buff, int maxlen);
#define http_set_uri_sep(pctx,s_path,s_param,s_pkv) \
    (pctx)->spliter_path= s_path;\
    (pctx)->spliter_param = s_param;\
    (pctx)->spliter_pkv = s_pkv;
#define http_set_memory_msuit(pctx,pmalloc,pfree,pextend) \
    (pctx)->hmalloc = (pmalloc);\
    (pctx)->hfree = (pfree);\
    (pctx)->hextend = (pextend);

extern int http_extend(lx_buffer * buff);
extern int http_ctx_init(h_parser_ctx * pctx,h_type_t type,int init_buff_size);
extern int http_ctx_cleanup(h_parser_ctx * pctx);

extern int http_hinfo_cleanup(h_parser_ctx * pctx);

extern int http_parse_uri(h_parser_ctx * pctx);
extern int http_parse(h_parser_ctx * ctx);

extern int http_set_headers(h_parser_ctx * pctx,char ** headers, int contlen);

#endif
