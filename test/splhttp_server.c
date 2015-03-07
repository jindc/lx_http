#include <stdlib.h>
#include "splhttp_server.h"
#include "lxtime.h"
#include "lx_http.h"
#include "lx_http_util.h"

#define g_ctx (g_splhttp_server_ctx)

struct splhttp_hdarg
{
    lx_connection conn;
};
typedef struct splhttp_hdarg splhttp_hdarg; 

struct splhttp_server_ctx 
{
    int stimeo_milli;
    int rtimeo_milli;
    lx_bool_t is_nostub;
};
typedef struct splhttp_server_ctx splhttp_server_ctx;

static splhttp_server_ctx *g_splhttp_server_ctx;

static char * g_home = "home";
static char * g_whome = "webhome";

int init_splhttp_server(lx_bool_t is_nostub)
{
    char buff[1024];

    struct splhttp_server_ctx * ctx;
    ctx = (splhttp_server_ctx *)malloc(sizeof(splhttp_server_ctx));
    if( ctx == NULL)
    {
        perror("malloc in init error\n");
        return -1;
    }
    ctx->stimeo_milli = 200000;
    ctx->rtimeo_milli = 200000;
    ctx->is_nostub = is_nostub;

    g_splhttp_server_ctx = ctx;
    
    snprintf(buff, 1024,"mkdir -p %s/%s",g_home,g_whome);
    system(buff);

    return 0;
}

int cleanup_splhttp_server()
{
    if( g_ctx != NULL)
    {
        free(g_ctx); 
        g_ctx = NULL; 
    }    
    return 0;
}
/*just save request */
static int handler(void *);

int start_splhttp_server(int port)
{
    int listen_fd;
    splhttp_hdarg arg;

    if( (listen_fd = lx_listen(port)) == -1)
    {
        perror("lx_listen error\n");
        return -1;
    }
    
    if(lx_start_server(listen_fd,handler,&arg ))
    {
        perror("lx_start_server error");
        return -1;
    }
    return 0;
}

static int recv_req(int fd, lx_bool_t is_nostub,h_parser_ctx * pctx);
static int send_resp(int fd, lx_bool_t is_nostub,h_parser_ctx * pctx);

static int handler(void * arg)
{
    splhttp_hdarg *harg = (splhttp_hdarg*)arg;
    int fd,ret;
    char buff[4096];
    h_parser_ctx ctx;

    const char * reqfname = "request.txt";
    const char * resfname = "response.txt";

    printf("begin\n");
    
    fd = harg->conn.fd;
    if(lx_set_timeo(fd,g_ctx->stimeo_milli,g_ctx->rtimeo_milli))
    {
        perror("lx_set_timeo error\n");
        ret = -1;
        goto end;
    }
    
    http_set_uri_sep(&ctx,'?','&','=');
    http_set_memory_msuit(&ctx,malloc,free,http_extend);
 
	if( http_ctx_init(&ctx,T_REQ,64)){
        err_ret("init parser ctx error");
        ret = -1;goto end;
    }

    if(recv_req(fd,g_ctx->is_nostub,&ctx)){
        perror("recv_req error");
        ret = -1; goto end;
    }

    if(http_print_http(&ctx)){
        err_ret("print_pare_info error");
        ret = -1;goto end;
    }

    if(send_resp(fd,g_ctx->is_nostub,&ctx)){
        perror("send resp error");
        ret = -1; goto end;
    }

    ret = 0;

end:
    if(fd != -1){
        close(fd);
        fd = -1;
        harg->conn.fd = -1;
    }   
    http_ctx_cleanup(&ctx);
    
    printf("end\n");
    return ret;
}

static int recv_req(int fd,lx_bool_t is_nostub, h_parser_ctx * pctx)
{
    int ret,read_num;
    h_head_info * pinfo;

    FILE * fh = NULL;
    char *path = "request.stub";

    http_set_uri_sep(pctx,'?','&','=');
    http_set_memory_msuit(pctx,malloc,free,http_extend);
    pinfo  = &pctx->info;
    
    if(!is_nostub &&
        ( (fh = fopen(path,"wb")) == NULL) ){
        err_ret("open stub file error:%s",path);
        ret = -1;goto err;
    }

	while(1)
	{
		read_num = recv(fd,lx_buffer_lenp((&pctx->orig_buff))
			,lx_buffer_freenum((&pctx->orig_buff)),0);
		if( read_num < 0){
            if(errno == EINTR )
                continue;
			err_ret("recv error"),ret =-1;goto err;
        }else if( read_num == 0){
			ret = -1;
			err_ret("cannot get enough head info");goto err;
		}

		if(!is_nostub){
            if(fwriten(fh,lx_buffer_lenp( &pctx->orig_buff), read_num) != read_num){    
			    err_ret("write stub file error"),ret =-1;goto err;
            }    
        }

        pctx->orig_buff.len += read_num;
		
		ret = http_parse(pctx);
		if( ret == HEC_OK){
			break;	
		}else if( ret == HEC_NEED_MORE)
			;
		else{
			err_ret("parser error[%d]",ret);goto err;
			ret = -1;
		}
	}

    if(http_save_body(fd, fh,"request_boby" ,pctx,LX_TRUE) ){
        err_ret("save body error,%s","request_body");
        ret = -1;goto err;
    }

    ret = 0;
err:
    if(fh)
        fclose(fh);

    return ret;   
}

static int send_resp(int fd,lx_bool_t is_nostub, h_parser_ctx * req_ctx)
{
    int ret,head_len,contlen = -1, rcode;
    h_parser_ctx ctx,*pctx;
    char path[1024],date[64],buff[4096],* uri,*rstr;
    FILE * resp_fh = NULL,*stub_fh = NULL;
  
    char * headers[] = {  
       // "Date"          ,"Mon, 26 Jan 2015 08:52:12 GMT",
        "Content-Type" ,"text/html",
        "Connection"   ,"Keep-Alive",
        "Server"       ,"lanxin/spl 1.0",
        NULL,NULL
    };
    
    if( !is_nostub 
        &&(stub_fh = fopen("response.stub","wb")) ==NULL ){
        err_ret("open response.stub file error");
        ret = -1; goto err;
    }
    
    http_set_uri_sep(&ctx,'?','&','=');
    http_set_memory_msuit(&ctx,malloc,free,http_extend);
    pctx = &ctx;
    
    if(http_ctx_init(&ctx,T_RESP,64)){
        err_ret("http_ctx_init error");
        ret =-1;goto err;
    }
    
    if( !(ret = get_browser_time( time(NULL),date,64 ) ) ){
        err_ret("snprintf date error");
        ret =-1;goto err;
    }

    uri = http_get_uri(&req_ctx->info);
    if( uri == NULL || strcmp(uri, "/") == 0)
        uri = "/index.html";
    if( (ret = snprintf(path,1024,"%s/%s%s",g_home,g_whome,uri)) <= 0 ){
        err_ret("snprintf path error,ret = %d",ret);
        ret =-1;goto err;
    }
    
    if( (resp_fh = fopen(path, "rb")) == NULL){
        rcode = 404;
        rstr = "File Not Found";
        
        if( (ret = snprintf(path,1024,"%s/%s%s",g_home,g_whome,"/404.html")) <= 0 ){
            err_ret("snprintf path error,ret = %d",ret);
            ret =-1;goto err;
        }
        if( (resp_fh= fopen(path,"rb"))== NULL){
            err_ret("open 404 file error:%s",path);
            ret =-1;goto err;
        }
    }else{
        rcode = RESP_OK;
        rstr = NULL;
        if( fseek(resp_fh,0,SEEK_END)== -1 
            ||(ret = ftell(resp_fh)) == -1){
            err_ret("get file len error");
            ret = -1; goto err;    
        }
        rewind(resp_fh);
        contlen = ret;
    }
    
    if( http_set_prot(pctx,P_HTTP_1_1))
    {
        err_ret("set prot error");
        ret = -1;goto err;
    }

    if( http_set_rcode(pctx,rcode,rstr))
    {
        err_ret("set resp code error");
        ret = -1;goto err;
    }

    if(http_set_headers(pctx,headers,contlen )){
        err_ret("set headers error");
        ret = -1; goto err;
    }
    
    if(http_set_header(pctx,"Date",date )){
        err_ret("set headers error");
        ret = -1; goto err;
    }

    if((head_len = http_seri_head(&pctx->info,T_RESP,buff,4096)) <= 0){
        err_ret("http_ctx_serihead error");
        ret = -1;goto err;
    }
   
    if(lx_sosend(fd,buff,head_len)!=head_len
        || (!is_nostub && fwriten(stub_fh,buff,head_len) != head_len)){
        err_ret("write or send  error");
        ret = -1;goto err;
    }

    while(resp_fh && (ret = freadn(resp_fh,buff,4096)) > 0 ){
        if( lx_sosend(fd,buff,ret)!= ret 
            || (!is_nostub && fwriten(stub_fh, buff,ret) != ret)){
            
            err_ret("write or send response  error");
            ret = -1;goto err;
        }
    } 

    ret = 0;
err:
    http_ctx_cleanup(pctx);
    if(resp_fh)
        fclose(resp_fh);
    if(stub_fh)
        fclose(stub_fh);
    return ret;
}
