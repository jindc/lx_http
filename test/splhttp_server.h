#ifndef SPLHTTP_SERVER_H 
#define SPLHTTP_SERVER_H 
#include "lx_types.h"
#include "lx_socket.h"
int init_splhttp_server( lx_bool_t is_nostub);

int cleanup_splhttp_server();

int start_splhttp_server(int port);

#endif
