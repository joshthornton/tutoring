#include <hiredis.h>
#include <fcgiapp.h>
#include <string.h>
#include <stdlib.h>
#include "session.h"
#include "../common/common.h"

char * session( redisContext *c, FCGX_ParamArray envp, char sid[33] )
{
	// Get cookies
	char *cookie = FCGX_GetParam( "HTTP_COOKIE", envp );
	if ( cookie ) {
		param *p;
		ssize_t num = convert_to_params( &p, cookie, "; ", "=", -1 );

		// Find session id
		while ( num-- )
			if ( !strcmp( p[num].name, "sid" ) ) {
				memcpy( sid, p[num].value, 33 );
				break;
			}

		// Lookup session id
		redisReply *r = redisCommand( c, "GET tute:session:%s", sid );
		if ( r->type != REDIS_REPLY_STRING )
		{
			freeReplyObject( r );
			return NULL;
		}
		char *user = malloc( sizeof( char) * r->len + 1 );
		memcpy( user, r->str, r->len );
		user[r->len] = '\0';
		freeReplyObject( r );

		// Reset TTL
		r = redisCommand( c, "EXPIRE tute:session:%s %d", sid, SESSION_DURATION );
		if ( r->type == REDIS_REPLY_ERROR )
		{
			freeReplyObject( r );
			free( user );
			return NULL;
		}

		freeReplyObject( r );
		return user;

	}
	
	return NULL;
}
