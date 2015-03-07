#include "lx_http_util.h"

int http_print_http(h_parser_ctx * pctx)
{
	char res_buff[4096];
    char * temp_buff;
    lx_kvlist_node *pnode;
    int ret,content_len;
    h_head_info * pinfo;

    pinfo = &pctx->info;
	if(http_seri_head(pinfo,pctx->type,res_buff,4096)  <= 0)
	{
		err_ret("str_head_info error");
		ret = -1;goto err;
	}
	//printf("the head info is:\n%s\n",res_buff);
   
   if(pctx->type == T_REQ){
        printf("head info:\n"
            "method:[%s]\n" "uri:[%s]\n" "prot:[%s]\n"
            ,http_get_mtod(pinfo), http_get_uri(pinfo),http_get_prot(pinfo));
    }else{
        printf("get resp info:\n"
            "prot:[%s]\n" "code:[%d]\n" "code_str:[%s]\n"
            ,http_get_prot(pinfo), http_get_code(pinfo),http_get_codestr(pinfo));
        
    }
    printf("headers:\n");
    for(http_header_begin(pinfo,pnode);NULL !=pnode; http_header_next(pinfo,pnode))
    {
        printf("key:[%s] value:[%s]\n"
            ,http_get_buffval(pinfo,&pnode->key),http_get_buffval(pinfo, &pnode->value));    
    } 

    temp_buff = http_get_header(pinfo,"Accept-Encoding") ;
    //printf("get Accept-Encoding:%s\n",temp_buff== NULL?"no null":temp_buff);
    if(pctx->type == T_REQ){
        printf("path:%s\nparams:\n",http_get_path(pinfo));
        for(http_param_begin(pinfo,pnode); NULL !=pnode; http_param_next(pinfo,pnode) ){
            printf("key:[%s] value:[%s]\n"
                ,http_get_parambuff(pinfo,&pnode->key), http_get_parambuff(pinfo,&pnode->value));    
        }
        temp_buff = http_get_param(pinfo,"user");
     //   printf("get param user:%s\n",temp_buff== NULL?"no null":temp_buff);
    }

	content_len =http_get_contlen(pinfo);
	//printf("the content len is %d\n",content_len);

    ret = 0;
err:
    return ret;
}

int parse_url(char * url, h_url_s * psurl)
{
	char * tmp,*to_pars;
	
	to_pars = url;
	memset(psurl,0,sizeof(*psurl));
	psurl->port = -1;
	psurl->pthidx = -1;
	if( (tmp = strstr(to_pars,"://")) ){
		*tmp = 0;
		psurl->sche = to_pars;
		to_pars = tmp + strlen("://");
	}
	
	psurl->host = to_pars;
	if( (tmp = strchr(psurl->host,'/')))
	{
		*tmp = 0;
		psurl->path = tmp +1;
		psurl->pthidx = tmp - url; 
	}
	
	//parse host and port
	if( tmp = strchr(psurl->host, ':')){
		psurl->port = atoi(tmp+1);
		*tmp = 0;
	}
    
    if(! psurl->path)
        goto ret;

	//parse path and resource and params
	if( psurl->path && (tmp = strchr(psurl->path,'?')) ){
		*tmp = 0;
		if(strlen(tmp+1) >0)
			psurl->params = tmp +1;
    }

    if(tmp = strrchr(psurl->path, '/')){
        *tmp = 0;
        psurl->rsour = tmp +1;
    }else{
        psurl->rsour = psurl->path;
        psurl->path = NULL;
    } 

ret:
    if(psurl->port < 0)
        psurl->port = 80;

	if(psurl->host == NULL)
		return -1;
	
	return 0;
}    

void test_parurl(char * url)
{
	char orig_url[] = "http://www.baidu.com:80/s?word=url&tn=sitehao123&ie=utf-8";
    if(!url)
        url = orig_url;
	h_url_s surl;
	if(parse_url(url, &surl))
		err_quit("parse url error");
	else{
		printf("sche:[%s]\nuser[%s]\npwd[%s]\nhost[%s]\nport[%d]\n"
		"path[%s]\nrsour[%s]\nparams[%s]\n",
		lx_null2str(surl.sche),lx_null2str(surl.user),lx_null2str(surl.pwd),
		lx_null2str(surl.host),surl.port,
		lx_null2str(surl.path),lx_null2str(surl.rsour),lx_null2str(surl.params)
		);
	}	
}

int http_save_body(int fd, FILE * stubfh, char * prefix,h_parser_ctx * pctx,lx_bool_t is_req)
{
    int ret,content_len,nleft,blen,haslen;
    lx_buffer data;
    FILE * fh = NULL;
    char path[1024];
    char pbuff[4096];
    char * buff = NULL;

	content_len =http_get_contlen(&pctx->info);
	//printf("the content len is %d\n",content_len);

    if(content_len < 0 &&is_req)
        content_len = 0;

    snprintf(path,1024,"%s",prefix);
    if( (fh = fopen(path,"wb")) == NULL){
        err_ret("open body file:%s error",path);
        return -1;
    }

    blen = pctx->orig_buff.maxlen - pctx->orig_buff.offset;
    haslen = lx_buffer_unscannum(&pctx->orig_buff);
    if(blen < 4096){
        buff = pbuff;
        blen = 4096;
        memcpy(buff,lx_buffer_offsetp(&pctx->orig_buff),haslen);
    }else{
        buff = lx_buffer_offsetp(&pctx->orig_buff);
    }

    lx_buffer_init(&data,buff,0,haslen, blen);

	nleft = content_len - data.len;
	do{
		if( fwriten(fh,data.base,data.len) != data.len){
            err_ret("write body into file:%s error",path);
            ret = -1; goto quit;
         }
        
        if(content_len < 0  || nleft >0){
			ret = recv(fd,data.base, (nleft>0&&nleft < data.maxlen)?nleft:data.maxlen,0  );
			if(ret < 0){
                if(errno == EINTR )
                    ret =0,data.len = ret;
                else{
                    err_ret("recv body error"); 
                    ret = -1; goto quit;
                }
            }else if(ret == 0){
                err_ret("recv end");
                ret = content_len > 0 ? -1: 0;
                goto quit;
            }else{
    			data.len = ret;
                printf("recv %d\n",content_len - nleft );
                if(stubfh && fwriten(stubfh,data.base,data.len) != data.len ){
                    err_ret("write body into stub file error");
                    ret = -1;goto quit;
                }
            }
		}else
			break;
		nleft -= ret;
	}while(1);

    ret = 0;
quit:
    if(fh)
        fclose(fh);
    return ret;
}


