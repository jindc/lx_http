#ifndef LX_HTTP_UTIL_H
#define LX_HTTP_UTIL_H
#include "lx_http.h"
#include "lx_serror.h"

typedef struct h_url_s h_url_s;
struct h_url_s{
    char * sche;
    char * user;
    char * pwd;
	char * host;
	int port;
	
    char * path;
    char * rsour;
    char * params;
	
	char * orip;
	int pthidx;
};

int parse_url(char * url, h_url_s * psurl);

void test_parurl(char * url);

int http_print_http(h_parser_ctx * pctx);
int http_save_body(int fd, FILE * stubfh, char * prefix,h_parser_ctx * pctx,lx_bool_t is_req);

#endif
