#include "splhttp_client.h"
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <string.h>
#include <lx_socket.h>
#include "lx_serror.h"
#include "lx_fileio.h"


int spl_connect(int * fd, h_url_s * psurl )
{
    struct hostent * hent;
    struct in_addr * paddr;
    int ret;
    char * host;
    struct sockaddr_in addr;

    if((hent = gethostbyname(psurl->host) ) == NULL){
        err_ret("get host by name error");
        herror(psurl->host);
        return -1;
    }
    host = inet_ntoa(*((struct in_addr *)hent->h_addr ));
    printf("host[%s]:%s:%s\n", psurl->host,hent->h_name,host);
    
    if( (*fd = socket(PF_INET,SOCK_STREAM,0)) < 0){
       err_ret("socket() error");
       return -1;
    }

    memset(&addr,0,sizeof(addr)); 
    addr.sin_family = PF_INET;
    addr.sin_port = htons(psurl->port);
    addr.sin_addr.s_addr = inet_addr(host);
    printf("host:%s\n",inet_ntoa(addr.sin_addr ));
    if( (ret = connect(*fd,(struct sockaddr *)&addr,sizeof(addr) )) < 0)
        goto quit;    
    if(lx_set_timeo(*fd,5000,5000))
        goto quit;

   return 0;
quit:
    err_ret("spl_connect err");
    close(*fd);
    return -1;    
}

int spl_send(int fd,lx_bool_t is_nostub ,h_url_s * psurl,char * body)
{
    char buff[4096];
    int ret,head_len,contlen;
    h_parser_ctx ctx,*pctx;
    char * uri;
    h_method_t mtod;

    char * headers[] = {  
        "Accept","text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8",
        "User-Agent", "Mozilla/5.0 (Windows NT 6.1) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/31.0.1650.63 Safari/537.36",
        "Accept-Encoding","deflate,sdch",
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
    
    contlen = body?(int)strlen(body):-1;
    mtod = contlen >= 0?M_POST:M_GET; 
    if(  http_set_mtod(pctx,mtod))
    {
        err_ret("set method error");
        ret = -1;goto err;
    }

    if( (psurl->pthidx > 0 && http_set_uri(pctx, psurl->orip+psurl->pthidx ))
        || http_set_uri(pctx,"/"))
    {
        err_ret("set uri error");
        ret = -1;goto err;
    }

    if( http_set_prot(pctx,P_HTTP_1_1))
    {
        err_ret("set prot error");
        ret = -1;goto err;
    }

    if(http_set_headers(pctx,headers,contlen )){
        err_ret("set headers error");
        ret = -1; goto err;
    }
    
    if(http_set_header(pctx,"Host",psurl->host )){
        err_ret("set headers error");
        ret = -1; goto err;
    }

    if((head_len = http_seri_head(&pctx->info,T_REQ,buff,4096)) <= 0){
        err_ret("http_ctx_serihead error");
        ret = -1;goto err;
    }
   
    if(lx_sosend(fd,buff,head_len)!=head_len
        || ( contlen > -1 && lx_sosend(fd,body,strlen(body)) != (int)strlen(body))){
        err_ret("write response hd or body error");
        ret = -1;goto err;
    }
    
    if(!is_nostub){
        char path[1024];
        size_t hlen,blen;
        snprintf(path,1024,"%s.request.stub",psurl->host);
        
        hlen = head_len,blen = contlen;
        if(lx_writefile(path,LX_FALSE,buff,&hlen) != 0
            || ( contlen > -1 && lx_writefile(path,LX_TRUE,body,&blen) != 0 )){
            err_ret("write response hd or body to stub file error");
            ret = -1;goto err;
        }
    }

    ret = 0;
err:
    http_ctx_cleanup(pctx);
    return ret;
}
int spl_recv(int fd,lx_bool_t is_nostub,h_url_s * psurl)
{
    int ret,read_num;
    h_head_info * pinfo;
    h_parser_ctx ctx,*pctx;

    FILE * fh = NULL;
    char path[1024];
    snprintf(path,1024,"%s.response.stub",psurl->host);

    http_set_uri_sep(&ctx,'?','&','=');
    http_set_memory_msuit(&ctx,malloc,free,http_extend);
    pctx = &ctx;
    
    if(http_ctx_init(pctx,T_RESP,64)){
        err_ret("http_ctx_init error");
        ret =-1;goto err;
    }
    pinfo  = &pctx->info;
    
    if(!is_nostub &&
        ( (fh = fopen(path,"wb")) == NULL) ){
        err_ret("open response stub file error:%s",path);
        ret = -1;goto err;
    }

	while(1)
	{
		read_num = recv(fd,lx_buffer_lenp((&pctx->orig_buff))
			,lx_buffer_freenum((&pctx->orig_buff)), 0);
		if( read_num < 0){
            if(errno == EINTR){
                read_num = 0;
                continue;
            }
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

    if( ret = http_print_http(pctx)){
			err_ret("parser error[%d]",ret);goto err;
			ret = -1;
    }

    if(http_save_body(fd, fh,psurl->rsour? psurl->rsour:psurl->host ,pctx,LX_FALSE) ){
        err_ret("save response body error");
        ret = -1;goto err;
    }

    ret = 0;
err:
    if(fh)
        fclose(fh);

    http_ctx_cleanup(pctx);
    return ret;
}

int splhttp_client(char * ourl, lx_bool_t is_nostub)
{
    int ret = 0, fd = -1;
    char * url;
    h_url_s surl;

    if( !(url= (char *)malloc(strlen(ourl) +1)))
        err_quit("cannot malloc url");
    memcpy(url,ourl,strlen(ourl)+1);

	test_parurl(url);
    memcpy(url,ourl,strlen(ourl)+1);
    if( parse_url(url,&surl)){
        err_ret("parse url [%s] error",url);
        ret = -1; goto quit; 
    }
    surl.orip = ourl;

	//get ip by host and connect
    if( ret = spl_connect( &fd,&surl)){
        err_ret("get connect error"); 
        ret = -1;goto quit;
    }

    if( ret = spl_send(fd,is_nostub,&surl,NULL)  ){
        err_ret("send request error,ret = %d",ret);    
        ret = -1;goto quit;
    }
    //build and send and save request

	//parse and save response
    if( ret = spl_recv(fd,is_nostub,&surl)){
        err_ret("recv response error,ret = %d",ret);    
        ret = -1;goto quit;
    }

    ret = 0;
quit:
    if(fd >=0)
        close(fd);
    if(url)
        free(url);

	return ret;
}

