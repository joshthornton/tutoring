#ifndef _SESSION_POST_H_
#define _SESSION_POST_H_

#include <fcgiapp.h>
#include <unistd.h>
#include <hiredis.h>

char * session( redisContext *c, FCGX_ParamArray envp, char sid[33] );

#endif
