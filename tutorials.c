#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hiredis.h>
#include <fcgiapp.h>
#include "../common/common.h"
#include "../common/json.h"
#include "session.h"

#define CMD_SIZE (32)

void process( redisContext *c )
{

	FCGX_Stream *in, *out, *err;
	FCGX_ParamArray envp;
	redisReply *r;
	
	json reason = { .type = STRING, .name = "reason", .str = NULL };
	json * failureEls[1] = { &reason };
	json success = { .type = OBJECT_ARRAY, .length = 0, .name = NULL, .objArr = NULL }; 
	json failure = { .type = OBJECT, .length = 1, .name = NULL, .objArr = failureEls }; 

	while ( FCGX_Accept( &in, &out, &err, &envp ) >= 0 )
	{
		
		// Check method
		if ( check_method_get( out, envp ) )
			continue;

		// Check user is logged in
		char sid[33];
		char *user = session( c, envp, sid );
		if ( !user )
		{
			header( out, 401, NULL );
			reason.str = "Not logged in.";
			json_out( out, &failure );
			continue;
		}

		// Initialize reused variables	
		reason.str = "";
		success.length = 0;
		success.objArr = NULL;

		// Get user's tutes
		r = redisCommand( c, "SMEMBERS tute:tutor:%s", user );
		if ( r->type != REDIS_REPLY_ARRAY )
		{
			header( out, 500, NULL );
			reason.str = "DB error.";
			json_out( out, &failure );
			freeReplyObject( r );
			goto err;
		}

		// For each tute ouput details
		success.length = 0;
		success.objArr = malloc( sizeof( json * ) * r->elements );
		memset( success.objArr, 0, r->elements );

		for ( int i = 0; i < r->elements; ++i )
		{
			if ( r->element[i]->type == REDIS_REPLY_STRING )
			{
				// Get tute details
				redisReply *r2 = redisCommand( c, "HMGET tute:tute:%s date start finish", r->element[i]->str );
				if ( r2->type != REDIS_REPLY_ARRAY || r2->elements != 3 ||
					r2->element[0]->type != REDIS_REPLY_STRING ||
					r2->element[1]->type != REDIS_REPLY_STRING || 
					r2->element[2]->type != REDIS_REPLY_STRING )
				{
					freeReplyObject( r2 );
					continue;
				}

				// Get tute course codes
				redisReply *r3 = redisCommand( c, "SMEMBERS tute:tute:%s:courses", r->element[i]->str );
				if ( r3->type != REDIS_REPLY_ARRAY ) 
				{
					freeReplyObject( r2 );
					freeReplyObject( r3 );
					continue;
				}
				for ( int j = 0; j < r3->elements; ++j )
					if ( r3->element[j]->type != REDIS_REPLY_STRING )
					{
						freeReplyObject( r2 );
						freeReplyObject( r3 );
						goto next;
					}

				// Create output object
				json *tute = malloc( sizeof( json ) );
				tute->type = OBJECT;
				tute->length = 4; 
				tute->objArr = malloc( sizeof( json * ) * tute->length );
				tute->name = NULL;
				success.objArr[i] = tute;
				success.length = i + 1;

				// Add date, start and finish
				json *date = malloc( sizeof( json ) ), *start = malloc( sizeof( json ) ), *finish = malloc( sizeof( json ) );
				date->type = start->type = finish->type = STRING;
				date->name = "date"; start->name = "start"; finish->name = "finish";
				date->str = malloc( sizeof( char ) * ( r2->element[0]->len + 1 ) );
				start->str = malloc( sizeof( char ) * ( r2->element[1]->len + 1 ) );
				finish->str = malloc( sizeof( char ) * ( r2->element[2]->len + 1 ) );
				memcpy( date->str, r2->element[0]->str, r2->element[0]->len ); date->str[r2->element[0]->len] = '\0';
				memcpy( start->str, r2->element[1]->str, r2->element[1]->len ); start->str[r2->element[1]->len] = '\0';
				memcpy( finish->str, r2->element[2]->str, r2->element[2]->len ); finish->str[r2->element[2]->len] = '\0';
				tute->objArr[0] = date; tute->objArr[1] = start; tute->objArr[2] = finish; 

				// Add course codes
				int length = 0;
				for ( int j = 0; j < r3->elements; ++j )
					length += r3->element[j]->len + 1; 
				json *courses = malloc( sizeof( json ) );
				courses->type = STRING;
				courses->name = "courses";
				courses->str = malloc( sizeof(char) * length ); memset( courses->str, ' ', length ); courses->str[length-1] = '\0';
				length = 0;
				for ( int j = 0; j < r3->elements; ++j )
				{
					memcpy( courses->str + length, r3->element[j]->str, r3->element[j]->len );
					length += r3->element[j]->len + 1; 
				}
				tute->objArr[3] = courses;

				freeReplyObject( r2 );
				freeReplyObject( r3 );

				next:
					continue;

			}
		}

		// Output success
		header( out, 200, sid );
		json_out( out, &success );
		
		//successErr:
			// Go through each tute 
			for ( int i = 0; i < success.length; ++i ) {
	
				if ( success.objArr[i] )
				{
					// Go through each attribute
					for ( int j = 0; j < success.objArr[i]->length; ++j ) {
						// free attribute string
						free( success.objArr[i]->objArr[j]->str );
						// free each attribute
						free( success.objArr[i]->objArr[j] );
					}
		
					// free each tute arr
					free( success.objArr[i]->objArr );
		
					// Free each tute 
					free( success.objArr[i] );
				}
			}

			// Free arr
			if ( success.objArr )
				free( success.objArr );

		err:
			free( user );
		
	}
}

int main ( int argc, char *argv[] )
{
	return init( &process );
}
