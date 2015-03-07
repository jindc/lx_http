#include "http_mock_test.h"

int mock_service(char * req_file, char * resp_file)
{
	FILE * req_fh = NULL, *resp_fh = NULL;
	h_parser_ctx ctx;
    int ret;

	http_set_uri_sep(&ctx,'?','&','=');
    http_set_memory_msuit(&ctx,malloc,free,http_extend);

	if( (req_fh = fopen(req_file,"r")) == NULL)
		err_quit("open file %s error",req_file);
 
	if( http_ctx_init(&ctx,T_REQ,64)){
        err_ret("init parser ctx error");
        ret = -1;goto err;
    }
    
    if(parse_http(req_fh,&ctx)){
        err_ret("parse http error");
        ret = -1;goto err;
    }

    if(http_print_http(&ctx)){
        err_ret("print_pare_info error");
        ret = -1;goto err;
    }

    if(drop_http_body(&ctx,req_fh)){
        err_ret("drop http body err");
        ret = -1;goto err;
    }

    resp_fh = fopen(resp_file,"w");
    if(resp_fh == NULL){
        err_ret("open response hd error");
        ret = -1;goto err;
    }


    if( send_resp(&ctx , resp_fh)){
        err_ret("send resp error");
        ret = -1;goto err;
    }

    ret = 0;
err:
    if(req_fh)
        fclose(req_fh);
    if(resp_fh)
        fclose(resp_fh);

    http_ctx_cleanup(&ctx);
	return ret;
}

int mock_client(char * req_file, char * resp_file)
{
	FILE * req_fh = NULL, *resp_fh = NULL;
	h_parser_ctx ctx;
    int ret;
   
    if((req_fh = fopen(req_file, "wb")) ==NULL){
        err_ret("open file %s error",req_file);
        ret = -1; goto err;
    }
    
    if( send_req(req_fh,NULL)){
        err_ret("send request faill ");
        ret = -1; goto err;
    }

    if((resp_fh = fopen(resp_file, "rb")) ==NULL){
        err_ret("open file %s error",resp_file);
        ret = -1; goto err;
    }

	http_set_uri_sep(&ctx,'?','&','=');
    http_set_memory_msuit(&ctx,malloc,free,http_extend);
 
	if( http_ctx_init(&ctx,T_RESP,64)){
        err_ret("init parser ctx error");
        ret = -1;goto err;
    }
  
    if(parse_http(resp_fh,&ctx)){
        err_ret("parse http error");
        ret = -1;goto err;
    }

    if(http_print_http(&ctx)){
        err_ret("print_pare_info error");
        ret = -1;goto err;
    }

    if(drop_http_body(&ctx,resp_fh)){
        err_ret("drop http body err");
        ret = -1;goto err;
    }

   ret = 0;
err:
    if(req_fh)
        fclose(req_fh);
    if(resp_fh)
        fclose(resp_fh);

    http_ctx_cleanup(&ctx);
	return ret;
}

int drop_body(FILE * fh, lx_buffer * data_buff,int body_len)
{
	int nleft;
	int ret;
	
	nleft = body_len - data_buff->len;
	do{
		printf("%d[%.*s]\n",data_buff->len,data_buff->len,data_buff->base);
		if(nleft >0){
			ret = freadx(fh,data_buff->base,data_buff->maxlen,32);
			if(ret <= 0)
				return -1;
			data_buff->len = ret;	
		}else
			break;
		nleft -= ret;
	}while(1);
	
	return 0;
}
	
int parse_http(void * fd ,h_parser_ctx * pctx){
    FILE* req_fh;
	int ret,read_num;
    h_head_info * pinfo;

    req_fh = (FILE *)fd;
    pinfo  = &pctx->info;
    
	while(1)
	{
		read_num = freadx(req_fh,lx_buffer_lenp((&pctx->orig_buff))
			,lx_buffer_freenum((&pctx->orig_buff)),32);
		if( read_num < 0)
			err_quit("read error");
		if( read_num == 0){
			ret = -1;
			err_ret("cannot get enough request");goto err;
		}
		//printf("%d[%.*s]\n",read_num,read_num,lx_buffer_lenp((&pctx->orig_buff)));
		printf("%.*s",read_num,lx_buffer_lenp((&pctx->orig_buff)));
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
    return 0;

err:
    return ret;
}	

int drop_http_body(h_parser_ctx *pctx,FILE * req_fh)
{
    int ret,content_len;
    lx_buffer data;
    h_head_info * pinfo;

    pinfo = &pctx->info;
	content_len =http_get_contlen(pinfo);
	printf("the content len is %d\n",content_len);

    if(content_len > 0)
	{
        lx_buffer_init(&data,lx_buffer_offsetp((&pctx->orig_buff))
			,0,pctx->orig_buff.len - pctx->orig_buff.offset,pctx->orig_buff.maxlen -pctx->orig_buff.offset);

		if(ret = drop_body(req_fh,&data,content_len) ){
			err_ret("get request body error[%d]",ret);
            return -1;
        }
	}

    return 0;
}

int send_resp(h_parser_ctx *rctx,FILE* resp_fh)
{
    char * resp = "<html><title>lx_http </title><body>simple response</body></html>";
    char err[4096];
    int ret,head_len;
    h_parser_ctx ctx,*pctx;
    
    char * headers[] = {  
        "Date"          ,"Mon, 26 Jan 2015 08:52:12 GMT",
        "Content-Type" ,"text/html",
        "Connection"   ,"Keep-Alive",
        "Server"       ,"lanxin/1.0",
        "Set-Cookie"    ,"BDSVRTM=0; path=/",
        NULL,NULL
    };

	http_set_uri_sep(&ctx,'?','&','=');
    http_set_memory_msuit(&ctx,malloc,free,http_extend);
    pctx = &ctx;
    if(http_ctx_init(pctx,T_RESP,64)){
        err_ret("http_ctx_init error");
        ret =-1;goto err;
    }

    if( http_set_prot(pctx,P_HTTP_1_1))
    {
        err_ret("set prot error");
        ret = -1;goto err;
    }

    if( http_set_rcode(pctx,RESP_OK,NULL))
    {
        err_ret("set resp code error");
        ret = -1;goto err;
    }

    if(http_set_headers(pctx,headers,strlen(resp))){
        err_ret("set headers error");
        ret = -1; goto err;
    }

    if((head_len = http_seri_head(&pctx->info,T_RESP,err,4096)) <= 0){
        err_ret("http_ctx_serihead error");
        ret = -1;goto err;
    }
   
    if(fwriten(resp_fh,err,head_len)!=head_len
        || fwriten(resp_fh,resp,strlen(resp)) != (int)strlen(resp))
    {
        err_ret("write response hd or body error");
        ret = -1;goto err;
    }

    ret = 0;
err:
    http_ctx_cleanup(&ctx);
    return ret;
}

int send_req(FILE* fh,char * body)
{
    char buff[4096];
    int ret,head_len,contlen;
    h_parser_ctx ctx,*pctx;
    char * uri = "/aa.txt?user=usr1pass=123456";
    char * headers[] = {  
        "Host","127.0.0.1:8989",
        "Accept","text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8",
        "User-Agent", "Mozilla/5.0 (Windows NT 6.1) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/31.0.1650.63 Safari/537.36",
        "Accept-Encoding","gzip,deflate,sdch",
        "Accept-Language","zh-CN,zh;q=0.8",
        "Connection"   ,"Keep-Alive",
        NULL,NULL
    };

	http_set_uri_sep(&ctx,'?','&','=');
    http_set_memory_msuit(&ctx,malloc,free,http_extend);
    pctx = &ctx;
    
    if(http_ctx_init(pctx,T_REQ,64)){
        err_ret("http_ctx_init error");
        ret =-1;goto err;
    }
    
    if( http_set_mtod(pctx,M_GET))
    {
        err_ret("set method error");
        ret = -1;goto err;
    }
    if( http_set_uri(pctx,uri))
    {
        err_ret("set uri error");
        ret = -1;goto err;
    }

    if( http_set_prot(pctx,P_HTTP_1_1))
    {
        err_ret("set prot error");
        ret = -1;goto err;
    }

    contlen = body?(int)strlen(body):-1;
    if(http_set_headers(pctx,headers,contlen )){
        err_ret("set headers error");
        ret = -1; goto err;
    }

    if((head_len = http_seri_head(&pctx->info,T_REQ,buff,4096)) <= 0){
        err_ret("http_ctx_serihead error");
        ret = -1;goto err;
    }
   
    if(fwriten(fh,buff,head_len)!=head_len
        || ( contlen > -1 && fwriten(fh,body,strlen(body)) != (int)strlen(body))
        )
    {
        err_ret("write response hd or body error");
        ret = -1;goto err;
    }
   

    ret = 0;
err:
    http_ctx_cleanup(pctx);
    return ret;
}


