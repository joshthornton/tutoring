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
	json success = { .type = OBJECT, .length = 0, .name = NULL, .objArr = NULL }; 
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

		// Get GET params
		char *s = url_decode( FCGX_GetParam( "QUERY_STRING", envp ) ); 

		// Parse GET params
		param *p;
		ssize_t num = convert_to_params( &p, s, "&", "=", -1 );
		
		// Get parameters
		char *courses = NULL;
		for ( int i = 0; i < num; ++i )
		{
			if ( !courses && !strcmp( p[i].name, "courses" ) ) {
				courses = p[i].value;
			}
		}

		// Check all parameters entered
		if ( !courses ) 
		{
			header( out, 400, NULL );
			reason.str = "Course codes not provided.";
			json_out( out, &failure );
			goto err;
		}

		// Parse codes
		param *course;
		num = convert_to_params( &course, courses, ";", ":", -1 ); 
		if ( num ) {

			ssize_t size = CMD_SIZE;
			ssize_t len = 6;
			char *cmd = malloc( sizeof( char ) * (size + 1) );	
			memcpy( cmd, "SUNION", len );

			while ( num-- )
			{
				// convert to lower
				to_lower( course[num].name );

				// Add course to cmd
				ssize_t l = strlen( course[num].name );

				// Increase size if necessary
				while ( len + l + 9 + 13 > size ) { // allow for full key
					size += size;
					cmd = realloc( cmd, sizeof( char ) * (size + 1) );
				}

				// Add key 
				memcpy( cmd + len, " tute:course:", 13 );
				len += 13;

				// Copy course code
				memcpy( cmd + len, course[num].name, l );
				len += l;

				// Copy end of key
				memcpy( cmd + len, ":students", 9 );
				len += 9;
				
				cmd[len] = '\0';
	
			}

			// Perform command
			r = redisCommand( c, cmd );
			free( cmd );

			// Parse results and output
			if ( r->type != REDIS_REPLY_ARRAY ) {
				header( out, 500, NULL );
				reason.str = "db error.";
				json_out( out, &failure );
				freeReplyObject( r );
				goto courseErr;
			}

			for ( int i = 0; i < r->elements; ++i )
				if ( r->element[i]->type != REDIS_REPLY_STRING ) {
					header( out, 500, NULL );
					reason.str = "db error.";
					json_out( out, &failure );
					freeReplyObject( r );
					goto courseErr;
				}

			// Allocate output array
			success.length = r->elements;
			success.objArr = malloc( sizeof( json * ) * success.length );
			memset( success.objArr, 0, success.length );

			// Go through each student and get their details
			for ( int i = 0; i < r->elements; ++i )
			{
				redisReply *r2 = redisCommand( c, "HMGET tute:student:%s first last college", r->element[i]->str );
				if ( r2->type != REDIS_REPLY_ARRAY && r2->elements != 3 &&
					r2->element[0]->type != REDIS_REPLY_STRING && 
					r2->element[1]->type != REDIS_REPLY_STRING && 
					r2->element[2]->type != REDIS_REPLY_STRING ) {
						header( out, 500, NULL );
						reason.str = "db error.";
						json_out( out, &failure );
						freeReplyObject( r2 );
						freeReplyObject( r ); 
						goto successErr;
				}

				// Create each students json object
				json *obj = malloc ( sizeof( json ) );
				obj->type = OBJECT;
				obj->length = 3;
				obj->objArr = malloc( sizeof( json * ) * 3 );
				obj->name = malloc( sizeof( char ) * ( r->element[i]->len + 1 ) );
				memcpy( obj->name, r->element[i]->str, r->element[i]->len );
				obj->name[r->element[i]->len] = '\0';
				success.objArr[i] = obj;

				// first name json
				json *first = malloc( sizeof( json ) );
				first->name = "first";
				first->type = STRING;
				first->str = malloc( sizeof( char ) * ( r2->element[0]->len + 1 ) );
				memcpy( first->str, r2->element[0]->str, r2->element[0]->len );
				first->str[r2->element[0]->len] = '\0';
				obj->objArr[0] = first;

				// last name json
				json *last = malloc( sizeof( json ) );
				last->name = "last";
				last->type = STRING;
				last->str = malloc( sizeof( char ) * ( r2->element[1]->len + 1) );
				memcpy( last->str, r2->element[1]->str, r2->element[1]->len );
				last->str[r2->element[1]->len] = '\0';
				obj->objArr[1] = last;

				// college name json
				json *college = malloc( sizeof( json ) );
				college->name = "college";
				college->type = STRING;
				college->str = malloc( sizeof( char ) * ( r2->element[2]->len + 1 ));
				memcpy( college->str, r2->element[2]->str, r2->element[2]->len );
				college->str[r2->element[2]->len] = '\0';
				obj->objArr[2] = college;

				freeReplyObject( r2 );
			}

			freeReplyObject( r );

		}

		// Output success
		header( out, 200, sid );
		json_out( out, &success );
		
		successErr:
			// Go through each student
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
		
					// free each student name 
					free( success.objArr[i]->name );
		
					// free each student arr
					free( success.objArr[i]->objArr );
		
					// Free each student
					free( success.objArr[i] );
				}
			}

			// Free arr
			if ( success.objArr )
				free( success.objArr );

		courseErr:
			free( course ); // free params structure

		err:
			free( user );
			free( p ); // Free params structure
			free( s ); // Free GET params
	}
}

int main ( int argc, char *argv[] )
{
	return init( &process );
}
