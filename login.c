#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hiredis.h>
#include <fcgiapp.h>
#include "../common/common.h"
#include "../common/json.h"
#include "session.h"

void process( redisContext *c )
{

	FCGX_Stream *in, *out, *err;
	FCGX_ParamArray envp;
	redisReply *r;
	
	json reason = { .type = STRING, .name = "reason", .str = NULL };
	json* failureEls[1] = { &reason };
	json success = { .type = OBJECT, .length = 0, .name = NULL, .objArr = NULL }; 
	json failure = { .type = OBJECT, .length = 1, .name = NULL, .objArr = failureEls }; 

	while ( FCGX_Accept( &in, &out, &err, &envp ) >= 0 )
	{
		
		if ( check_method_post( out, envp ) )
			continue;

		// Initialize reused variables	
		reason.str = "";

		// Get contentlength
		long len = content_length( envp );
		
		// Get post params
		char *s;
		if ( !read_stream( &s, in, len ) )
		{
			header( out, 400, NULL );
			reason.str = "No parameters provided";
			json_out( out, &failure );
			continue;
		}

		// Parse POST params
		param *p;
		ssize_t num = convert_to_params( &p, s, "&", "=", -1 );
		
		// Get parameters
		char *user = NULL, *pass = NULL;
		for ( int i = 0; i < num; ++i )
		{
			if ( !user && !strcmp( p[i].name, "user" ) ) {
				user = p[i].value;
            } else if ( !pass && !strcmp( p[i].name, "pass" ) ) { 
                pass = p[i].value;
            }
		}

		// Check all parameters entered
		if ( !user || !pass )
		{
			header( out, 400, NULL );
			reason.str = "All details must be entered.";
			json_out( out, &failure );
			goto err;
		}

		// urldecode
		inplace_url_decode( user );
		inplace_url_decode( pass );
		
		// Convert user to lowercase
		to_lower( user );

		// Get user password and salt
		r = redisCommand( c, "HMGET tute:user:%s pass salt", user );
		if ( !( r->type ==  REDIS_REPLY_ARRAY && r->elements == 2 &&
			r->element[0]->type == REDIS_REPLY_STRING &&
			r->element[1]->type == REDIS_REPLY_STRING ) )
		{
			header( out, 400, NULL );
			reason.str = "Invalid username of password.";
			json_out( out, &failure );
			freeReplyObject( r );
			goto err;
		}

		// Calculate hash
		char calcHash[65]; 
		char *dbHash = r->element[0]->str;
		char *salt = r->element[1]->str;
		get_hash( calcHash, pass, salt );
		
		if ( strcmp( calcHash, dbHash ) )
		{
			header( out, 400, NULL );
			reason.str = "Invalid username of password.";
			json_out( out, &failure );
			freeReplyObject( r );
			goto err;
		}
		freeReplyObject( r );

		// Generate session
		char session[33];
		generate_session( session );

		// Set Session
		r = redisCommand( c, "SET tute:session:%s %s", session, user );
		if ( r->type == REDIS_REPLY_ERROR )
		{
			header( out, 500, NULL );
			reason.str = "Unable to create session.";
			json_out( out, &failure );
			freeReplyObject( r );
			goto err;
		}
		freeReplyObject( r );

		// Set session expiry
		r = redisCommand( c, "EXPIRE tute:session:%s %d", session, SESSION_DURATION );
		if ( r->type == REDIS_REPLY_ERROR )
		{
			header( out, 500, NULL );
			reason.str = "Unable to set session expiry.";
			json_out( out, &failure );
			freeReplyObject( r );
			goto err;
		}
		freeReplyObject( r );

		// Output success
		header( out, 200, session );
		json_out( out, &success );
		
		err:
			free( p ); // Free params structure
			free( s ); // Free POST stream

	}
}

int main ( int argc, char *argv[] )
{
	return init( &process );
}
