#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hiredis.h>
#include <fcgiapp.h>
#include "../common/common.h"
#include "../common/json.h"

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
		char *user = NULL, *email = NULL, *pass = NULL, *first = NULL, *last = NULL;
		for ( int i = 0; i < num; ++i )
		{
			if ( !user && !strcmp( p[i].name, "user" ) ) {
				user = p[i].value;
			} else if ( !email && !strcmp( p[i].name, "email" ) ) { 
                email = p[i].value;
            } else if ( !pass && !strcmp( p[i].name, "pass" ) ) { 
                pass = p[i].value;
            } else if ( !first && !strcmp( p[i].name, "first" ) ) { 
                first = p[i].value;
            } else if ( !last && !strcmp( p[i].name, "last" ) ) { 
                last = p[i].value;
            }
		}

		// Check all parameters entered
		if ( !user || !email || !pass || !first || !last )
		{
			header( out, 400, NULL );
			reason.str = "All details must be entered.";
			json_out( out, &failure );
			goto err;
		}

		// url decode
		inplace_url_decode( user );
		inplace_url_decode( email );
		inplace_url_decode( pass );
		inplace_url_decode( first );
		inplace_url_decode( last );

		// Convert user to lowercase
		to_lower( user );
		
		// Check user key does not exist
		r = redisCommand( c, "HGETALL tute:user:%s", user );
		if ( ! ( r->type == REDIS_REPLY_NIL || ( r->type ==  REDIS_REPLY_ARRAY && r->elements == 0 ) ) )
		{
			header( out, 400, NULL );
			reason.str = "Username already exists.";
			json_out( out, &failure );
			freeReplyObject( r );
			goto err;
		}
		
		// Free reply object
		freeReplyObject( r );

		// Generate salt and hash password
		char hash[65];
		char salt[9];
		generate_salt( salt );
		get_hash( hash, pass, salt );

		// Save details to database
		r = redisCommand( c, "HMSET tute:user:%s email %s first %s last %s pass %s salt %s", user, email, first, last, hash, salt );
		if ( r->type == REDIS_REPLY_ERROR )
		{
			header( out, 500, NULL );
			reason.str = "Database error.";
			json_out( out, &failure );
			freeReplyObject( r );
			goto err;
		}
		
		// Free reply object
		freeReplyObject( r );
		
		// Output success
		header( out, 201, NULL );
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
