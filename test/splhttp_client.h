#ifndef SPLHTTP_CLIENT_H
#define SPLHTTP_CLIENT_H

#include "lx_http_util.h"

int splhttp_client(char * ourl, lx_bool_t is_nostub);

int spl_connect(int * fd, h_url_s * psurl );
int spl_send(int fd,lx_bool_t is_nostub ,h_url_s * psurl,char * body);
int spl_recv(int fd,lx_bool_t is_nostub ,h_url_s * psurl);


#endif
