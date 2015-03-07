#ifndef HTTP_MOCK_TEST_H
#define HTTP_MOCK_TEST_H

#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include "lx_serror.h"
#include "lx_fileio.h"
#include "lx_http.h"
#include "lx_types.h"

int mock_service(char * req_file, char * resp_file);
int mock_client(char * req_file, char * resp_file);

int drop_body(FILE * fh, lx_buffer * data_buff,int body_len);
int parse_http(void * fd ,h_parser_ctx * pctx);
int drop_http_body(h_parser_ctx* pctx,FILE * req_fh);
int send_resp(h_parser_ctx *pctx,FILE* resp_fh);

#endif


