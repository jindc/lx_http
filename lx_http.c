#include "lx_http.h"

char * http_resp_str[] ={
    "200", "OK", 
    "300", "redirection",
    "400", "client error",
    "500", "server error",
     NULL,  NULL
    };

int findarr_by_value( char ** array,char * value)
{
    int i = 0;

    while(*array)
    {
        if( strncmp(*array, value,strlen(*array)) == 0 )
            return i;
        ++i;
    }
    return -1;
}

char *h_mtod_str[] = {"GET","POST","HEAD",NULL};
char * h_prot_str[] ={"HTTP/0.", "HTTP/1.0","HTTP/1.1",NULL}; 
char * http_stage_str[] = 
{"stage_start","stage_listening","stage_req_head","stage_req_body","stage_resp_head","stage_resp_body","stage_done" };

int get_http_prot(char * buff,int len)
{
    if(strncmp(buff, h_prot_str[2],len ) == 0)
        return P_HTTP_1_1;
    else if( strncmp(buff,h_prot_str[1],len) ==0 )
        return P_HTTP_1_0;
    else if( strncmp(buff, h_prot_str[0], strlen( h_prot_str[0])) == 0)
        return P_HTTP_0_X;
    return -1;
}

int http_set_prop1(h_parser_ctx *pctx,int intval, char ** strarr,void * key, lx_buffer * str)
{
    char * nbuff,*temp_buff;
    int len;

    temp_buff = findarr_by_key(strarr,intval);
    len = strlen(temp_buff);

    if( (lx_buffer_reset(str,temp_buff,pctx->hmalloc,pctx->hfree)== NULL ))
        return HEC_MALLOC_ERR;
    
    *(int *)key = intval;
    return 0;
}

int http_set_prop2(h_parser_ctx *pctx, lx_buffer * buff, char * nval)
{

    if( (lx_buffer_reset(buff,nval,pctx->hmalloc,pctx->hfree)== NULL ))
        return HEC_MALLOC_ERR;
    
    return 0;
}

int http_set_rcode(h_parser_ctx * pctx,h_resp_code_t code, char * code_str)
{
    char * nbuff,*temp_buff;
    size_t len;

    if(code_str == NULL){
        code_str = http_get_respstr(code);
        if(!code_str)
            code_str = "";
    }

    len = strlen(code_str);
    if( lx_buffer_reset(&pctx->info.code_str,code_str,pctx->hmalloc,pctx->hfree ) == NULL )
        return HEC_MALLOC_ERR;

    pctx->info.resp_code = code;
  
    return 0;
}

lx_kvlist_node *http_find_node(h_head_info * info,char * key,lx_kvlist_node * head_node,lx_bool_t is_header)
{
	lx_kvlist_node * node;
    char * base;

    if(is_header)
        base = info->base;
    else
        base = info->uri_base;

	if( (node =kvlist_find(head_node,key,NULL,lx_base)) == NULL)
		node = kvlist_find(head_node,key,base,lx_ebase_offset);
		
	return node;
}

char * http_find_node_value(h_head_info *info,char * key,lx_kvlist_node * head_node,lx_bool_t is_header)
{
	lx_kvlist_node * node;
	
	node = http_find_node(info,key,head_node,is_header);
	if(node == NULL)
		return NULL;
	
	if(node->value.base)
		return node->value.base;
	else{
        if(is_header)
		    return info->base + node->value.offset;
        else
            return info->uri_base + node->value.offset;
    }
}

int http_set_prop3(h_parser_ctx * pctx,lx_kvlist_node ** phead_node ,char * key, char * value,lx_bool_t is_header)
{
    lx_kvlist_node * node;
    

    node = http_find_node(&pctx->info, key,*phead_node,is_header);
    if(!node){
        node =(lx_kvlist_node *)kvlist_append(phead_node,key,value,pctx->hmalloc,pctx->hfree); 
        if(node == NULL)
            return HEC_APPNODE_ERR;
    }else{
        if(lx_buffer_reset(&node->value,value,pctx->hmalloc,pctx->hfree)  == NULL)
            return HEC_MALLOC_ERR;
    }

    return 0;
}

int http_get_contlen(h_head_info *info)
{
    char * buff;

	buff = http_get_header(info,"Content-Length");
	if(buff == NULL)
		return -1;
	
	return atoi(buff);
}


int http_hinfo_cleanup(h_parser_ctx * pctx)
{
    h_head_info *pinfo = &pctx->info;
    lx_kvlist_node ** listpp[] = {&pinfo->params,&pinfo->headers}; 
    lx_kvlist_node * cur,*pre;
    
    lx_buffer * tofree[] = {&pinfo->mtod_str,&pinfo->uri,&pinfo->path, &pinfo->prot_str,&pinfo->code_str};
    int i;

    for(i = 0; i <(int) (sizeof(tofree)/sizeof(tofree[0]));++i ){
        if(tofree[i] ->base)
            pctx->hfree(tofree[i]->base);     
    }
    
    for(i = 0; i <(int)( sizeof(listpp)/sizeof(listpp[i])); i++ )
    {
        cur = *listpp[i];
        while(cur)
        {
            if(cur->key.base)
                pctx->hfree(cur->key.base);
            if(cur->value.base)
                pctx->hfree(cur->value.base);
            
            pre = cur;
            cur =(lx_kvlist_node *)cur->list.next;
            pctx->hfree(pre);
        }
        
        *listpp[i] = NULL;    
    }

    return 0;
}

int http_seri_head(h_head_info *pinfo,h_type_t type, char * buff, int maxlen)
{
    int ret;
    int len;
    lx_kvlist_node *node;

    len = 0;
    if(type == T_REQ)
        ret = snprintf(buff,maxlen,"%s %s %s\r\n"
            ,http_get_mtod(pinfo),http_get_uri(pinfo),http_get_prot(pinfo));
    else
       ret = snprintf(buff, maxlen,"%s %d %s\r\n"
            ,http_get_prot(pinfo) ,http_get_code(pinfo),http_get_codestr(pinfo));

    if(ret <= 0)
        return HEC_MEMORY_ERR;

    len +=ret;
    node = pinfo->headers;
    while(node)
    {
        ret = snprintf(buff+len, maxlen -len, "%s: %s\r\n"
            ,http_get_buffval(pinfo,&node->key), http_get_buffval(pinfo,&node->value));

        if(ret < 0)
            return HEC_MEMORY_ERR;
        len += ret;
        node =(lx_kvlist_node *)node->list.next;
    }
    
    if( (ret = snprintf(buff+len, maxlen -len, "\r\n")) < 0)
        return HEC_MEMORY_ERR;
    len += ret;

    return len;
}

int http_extend(lx_buffer * buff)
{
    char * nbuff;

    nbuff = (char *)realloc(buff->base,buff->maxlen * 2);
    if(nbuff){
        buff->base = nbuff;
        buff->maxlen = buff->maxlen * 2;
        return 0;
    }

    return -1;
}

int http_ctx_init(h_parser_ctx * pctx,h_type_t type,int init_buff_size)
{
    char * buff;

    pctx->type = type;
    
    pctx->last_stat = S_NONE;
    pctx->cur_stat = S_NONE;
    pctx->next_stat = S_NONE;
    
    memset(&pctx->uri_buff,0,sizeof(lx_buffer));
    memset(&pctx->orig_buff,0,sizeof(lx_buffer));
    memset(&pctx->temp_buff,0,sizeof(lx_buffer));

    memset(&pctx->info,0 ,sizeof(h_head_info));
    
    buff =(char *)pctx->hmalloc(init_buff_size);
    if(buff == NULL)
        return HEC_MALLOC_ERR;
    lx_buffer_init(&pctx->orig_buff,buff,0,0,init_buff_size);
    
    return 0;
}

int http_ctx_cleanup(h_parser_ctx * pctx)
{
    lx_buffer * tofree[] = { &pctx->orig_buff,&pctx->uri_buff}; 
    int i;
    
    for(i = 0; i <(int)( sizeof(tofree)/sizeof(tofree[0]));++i){
        if(tofree[i]->base)
        {
            pctx->hfree(tofree[i]->base);
            tofree[i]->base = NULL;
        };
    }
    pctx->info.base = NULL;
    pctx->info.uri_base = NULL;
    
    http_hinfo_cleanup(pctx);
    return 0;
}

int http_parse_uri(h_parser_ctx * pctx)
{
    h_head_info * pinfo;
    char * new_buff, * orig_uri, * param_begin, * param_end,*temp_buff;
    int len;
    lx_kvlist_node * param;

    pinfo = &pctx->info;

    orig_uri = pinfo->base + pinfo->uri.offset;
    len = strlen(orig_uri);
    if( (new_buff = (char *) pctx->hmalloc(len+1 ) ) == NULL)
        return HEC_MALLOC_ERR;
    memcpy(new_buff,orig_uri,len+1);
    lx_buffer_init(&pctx->uri_buff,new_buff,0,len+1,len+1);
    pinfo->uri_base = new_buff;
    
    pinfo->path.offset = 0;
    if( (temp_buff = strchr(new_buff,pctx->spliter_path) )== NULL)
        return 0;

    *temp_buff = 0;
    param_begin = temp_buff +1;
    while(param_begin){
        param =(lx_kvlist_node *)list_append((lx_list_node **)&pinfo->params,sizeof(lx_kvlist_node),pctx->hmalloc); 
        if(param == NULL)
            return HEC_APPNODE_ERR;

        param->key.offset = param_begin - new_buff;

        param_end = strchr(param_begin,pctx->spliter_param);
        if(param_end)
            *param_end = 0;

        temp_buff = strchr(param_begin,pctx->spliter_pkv);
        if(temp_buff == NULL)
            param->value.offset = param->key.offset + strlen(new_buff + param->key.offset);
        else{
            *temp_buff = 0;
            param->value.offset = temp_buff - new_buff  +1;
        }
        if(param_end == NULL){
            param_begin = NULL;
         }else{
            param_begin = param_end +1;
         } 
    }

    return 0;
};

int http_parse(h_parser_ctx * ctx)
{
    h_head_info * info;
    lx_buffer *  orig_buff;
    char * end, *buff,temp_buff;
    int parser_index,maxlen,tint;
    lx_kvlist_node * headers,*header ; 

 loop:
    orig_buff = &ctx->orig_buff;

    buff = orig_buff->base;
    maxlen = orig_buff->len;
    parser_index = orig_buff->offset;
    info = (h_head_info *)&ctx->info;

    if(orig_buff->offset >=  orig_buff->len )
        goto need_more;

    switch(ctx->cur_stat)
    {
    case S_NONE:

        if(ctx->type == T_REQ){
            ctx->cur_stat = S_REQ_METHOD;
        }else{
            ctx->cur_stat = S_RESP_PROTOCAL;
        }

        info->type = ctx->type;
        info->base = orig_buff->base;
        goto loop;

    case S_REQ_METHOD:
        
        //if(info->mtod_str.offset == 0)
          //  info->mtod_str.offset = parser_index;
        
        if((end = read2space(buff +parser_index , maxlen - parser_index ))
               == NULL )
            goto need_more;
        
        if( (tint=match_str(buff+ parser_index, h_mtod_str,sizeof(h_mtod_str)/sizeof(h_mtod_str[0]) -1 )  ) == -1)
            return HEC_INVALID_METHOD;
        info->mtod = tint;    
        orig_buff->offset = end - buff;
        ctx->cur_stat = S_ITEM_SPACE;
        ctx->next_stat = S_REQ_URI;
        goto loop;
        
    case S_REQ_URI:
         
        if(info->uri.offset == 0)
            info->uri.offset =  parser_index;
        
        if((end = read2space(buff + parser_index, maxlen - parser_index ))
               == NULL )
            goto need_more;
        
        orig_buff->offset = end - buff;
        ctx->last_stat = S_REQ_URI;
        ctx->cur_stat = S_ITEM_SPACE;
        ctx->next_stat = S_REQ_PROTOCAL;
        goto loop;
    
    case S_REQ_PROTOCAL:
        if(ctx->type ==T_REQ && info->prot_str.offset == 0)
            info->prot_str.offset = parser_index;
        
        if((end = read2nl(buff + parser_index , maxlen - parser_index ))
               == NULL )
            goto need_more;
        
        if( tint = get_http_prot(buff + parser_index,end - buff - parser_index) == -1)
            return HEC_INVALID_PROT;
        info->prot = tint;

        orig_buff->offset = end - buff;
                
        ctx->cur_stat = S_NEW_LINE;
        ctx->next_stat = S_HEADER_KEY;
        
        goto loop;

    case S_RESP_PROTOCAL: 
        //if(info->prot_str.offset == 0)
          //  info->prot_str.offset = parser_index;
        
        if((end = read2space(buff + parser_index , maxlen - parser_index ))
               == NULL )
            goto need_more;
        
        if( tint = get_http_prot(buff + parser_index,end - buff - parser_index) == -1)
            return HEC_INVALID_PROT;
        info->prot = tint;

        orig_buff->offset = end - buff;
        
        ctx->cur_stat = S_ITEM_SPACE;
        ctx->next_stat = S_RESP_CODE;
        
        goto loop;

    case S_RESP_CODE:
         if(ctx->temp_buff.offset == 0)
            ctx->temp_buff.offset = parser_index;
        
        if((end = read2space(buff +parser_index , maxlen - parser_index ))
               == NULL )
            goto need_more;
        
        if( ((tint = atoi(buff+ parser_index)) < 0))
            return HEC_INVALID_CODE;
        info->resp_code = tint;   

        orig_buff->offset = end - buff;
        ctx->cur_stat = S_ITEM_SPACE;
        ctx->next_stat = S_RESP_STR;
        goto loop;
        
    case S_RESP_STR:
         if(info->code_str.offset == 0)
            info->code_str.offset = parser_index;
        
        if((end = read2nl(buff + parser_index , maxlen - parser_index ))
               == NULL )
            goto need_more;
        
        orig_buff->offset = end - buff;
        
        ctx->cur_stat = S_NEW_LINE;
        ctx->next_stat = S_HEADER_KEY;
        goto loop;
 
    case S_HEADER_KEY:
        if(info->header_key.offset == 0)
            info->header_key.offset = parser_index;
        
        if( (end = read2str(buff+parser_index, maxlen - parser_index,HTTP_HEADER_SEP_STR)) == NULL)
            goto need_more;
        
        orig_buff->offset = end - buff;
        ctx->cur_stat = S_HEADER_SEP;
        goto loop;

    case S_HEADER_VALUE:
        if(info->header_value.offset == 0 )
            info->header_value.offset = parser_index;

        if((end = read2nl(buff + parser_index , maxlen - parser_index ))
               == NULL )
            goto need_more;
        
        header =(lx_kvlist_node *)list_append((lx_list_node **)&info->headers,sizeof(lx_kvlist_node),ctx->hmalloc); 
        if(header == NULL)
            return HEC_APPNODE_ERR;
       
        header->key =  info->header_key;   
        header->value = info->header_value;
        
        info->header_key.offset = 0;
        info->header_value.offset = 0;

        orig_buff->offset = end - buff;
        ctx->cur_stat = S_NEW_LINE;
        ctx->next_stat = S_HEADER_KEY; 
        
        goto loop;

    case S_HEADER_SEP:
        orig_buff->base[ orig_buff->offset] = 0;
        orig_buff->offset += strlen(HTTP_HEADER_SEP_STR);
        ctx->cur_stat = S_HEADER_VALUE;
        goto loop;

    case S_BODY:
        break;
    case S_ITEM_SPACE:
        orig_buff->base[ orig_buff->offset] = 0;
        orig_buff->offset++;

        if(ctx->last_stat == S_REQ_URI)
            if(http_parse_uri(ctx))
                return HEC_PARSE_URI_ERR;
        ctx ->last_stat = S_NONE;

        ctx->cur_stat = ctx->next_stat;
        goto loop;

    case S_NEW_LINE:
        if(maxlen - orig_buff->offset <(int)(2 * strlen(HTTP_NEW_LINE_STR)) )
            goto need_more;
        
        orig_buff->base[ orig_buff->offset] = 0;
        orig_buff->offset +=2;
        
        if( strncmp(orig_buff->base + orig_buff->offset
            , HTTP_NEW_LINE_STR,strlen(HTTP_NEW_LINE_STR ) ) == 0 )
        {
            orig_buff->offset += 2; 
            return HEC_OK;
        }

        ctx->cur_stat = ctx->next_stat;
        goto loop;
    default: 
        ;
    }

    return 0;    

need_more:
    if(orig_buff->len == orig_buff->maxlen)
    {
        if( ctx->hextend(orig_buff) )
            return HEC_EXTEND_ERR;
        info->base = orig_buff->base;    
    }
    return HEC_NEED_MORE;
};

int http_set_headers(h_parser_ctx * pctx,char ** headers, int contlen)
{
    char lenbuff[32];
    int ret,head_len;
    char ** ptemp_buff;
    
    if(contlen > 0){
        ret = snprintf(lenbuff,32,"%d",contlen);
        if(ret <= 0 )
            return HEC_MEMORY_ERR; 

        if(ret = http_set_header(pctx, "Content-Length",lenbuff)){
            return ret; 
        }
    }

    ptemp_buff = headers;
    while( *ptemp_buff)
    {
        if(ret = http_set_header(pctx,*ptemp_buff,*(ptemp_buff+1) )){
            return ret;
        }
        ptemp_buff += 2;
    }

    return 0;
}
