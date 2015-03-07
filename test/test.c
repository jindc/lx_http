#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include "lx_serror.h"
#include "lx_fileio.h"
#include "lx_http.h"
#include "lx_types.h"
#include "http_mock_test.h"
#include "splhttp_client.h"
#include "splhttp_server.h"

int main(int argc ,char * argv[])
{	
    lx_bool_t is_mock = LX_FALSE,is_server = LX_FALSE,is_nostub = LX_FALSE;
    int port = 8989,ret;
    char * host = NULL,  * uri = NULL, *req_file = NULL,*resp_file = NULL;
    struct option opts [] ={
        {"help",0,NULL,'h'},

        {"mock",0,NULL,'m'},
        {"server",0,NULL,'s'},
        {"client",0,NULL,'c'},
		 
        {"host",1,NULL,'1'},
        {"port",1,NULL,'p'},

        {"uri",1,NULL,'2'},

        {"req_file",1,NULL,'3'},
        {"resp_file",1,NULL,'4'},
		{"nostub",0,NULL,'5'},
		
        {NULL,0,NULL,0}
    };
    char * help = "usage:test [-h] [--mock] [-s] [-c] [--host] [--port] [--uri] [--req_file] [--resp_file]";
    
    while(1){
        char c ;
        int opt_index;
        
        c = getopt_long(argc,argv,"hmscp:",opts,&opt_index);
        if( c == -1)
            break;
        switch(c){
        case '?':
        case 'h':
        default :
            err_quit("%s",help);
            
        case 'm':
            is_mock = LX_TRUE;
            break;
        case 's':
            is_server = LX_TRUE;
            break;
        case 'c':
            is_server = LX_FALSE;
            break;
        case 'p':
            port = atoi(optarg);
            break;
        case '1':
            host = optarg;
            break;
        case '2':
            uri = optarg;
            break;
        case '3':
            req_file = optarg;
            break;
        case '4':
           resp_file = optarg;
           break;
		case '5':
			is_nostub = LX_TRUE;
			break;
        }   
    };
    printf("mock:%d,server:%d,req_file:%s,resp_file:%s\n",is_mock,is_server,req_file,resp_file);

    if(is_mock && is_server)
    {
        if(!req_file)
            req_file = "mtod_get.txt";
        if(!resp_file)
            resp_file = "gen_resp.txt";

        if(ret = mock_service(req_file, resp_file))
		    err_quit("mock service error:%d",ret);

    }else if(is_mock && !is_server){
        if(!req_file)
            req_file = "gen_get.txt";
        if(!resp_file)
            resp_file = "gen_resp.txt";
        
        if(ret = mock_client(req_file, resp_file))
		    err_quit("mock client error:%d",ret);
        
    }else if(!is_mock &&is_server){
        if(!host)
            host = "127.0.0.1";
        
        if(init_splhttp_server(is_nostub)){
            perror("init_splhttp_server error\n");
            return EXIT_FAILURE;
        }
        printf("init simple server ok\n");

        if(start_splhttp_server(port)){
            perror("start_splhttp_server error\n");
            return EXIT_FAILURE;
        }

        if(cleanup_splhttp_server()){
            perror("start_splhttp_server error\n");
            return EXIT_FAILURE;
        }

    }else{
        if(!uri)
			uri = "http://www.baidu.com:80/s?word=url&tn=sitehao123&ie=utf-8";
		if(ret = splhttp_client(uri, is_nostub))
		    err_quit("splhttp_client error:%d",ret);
    };

    return EXIT_SUCCESS;
}

