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
		
		// Check method
		if ( check_method_post( out, envp ) )
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
		char *courses = NULL, *date = NULL, *start = NULL, *finish = NULL, *students = NULL, *idStr = NULL;
		for ( int i = 0; i < num; ++i )
		{
			if ( !courses && !strcmp( p[i].name, "courses" ) ) {
				courses = p[i].value;
            } else if ( !date && !strcmp( p[i].name, "date" ) ) { 
                date = p[i].value;
            } else if ( !start && !strcmp( p[i].name, "start" ) ) { 
                start = p[i].value;
            } else if ( !finish && !strcmp( p[i].name, "finish" ) ) { 
                finish = p[i].value;
            } else if ( !students && !strcmp( p[i].name, "students" ) ) { 
                students = p[i].value;
			} else if ( !idStr && !strcmp( p[i].name, "id" ) ) {
				idStr = p[i].value;
			}
		}

		// Check all parameters entered
		if ( !courses || !date || !start || !finish || !students )
		{
			header( out, 400, NULL );
			reason.str = "All details must be entered.";
			json_out( out, &failure );
			goto err;
		}

		// URL decode components
		inplace_url_decode( courses );
		inplace_url_decode( date );
		inplace_url_decode( start );
		inplace_url_decode( finish );
		inplace_url_decode( students );

		// Get next tute id
		long long id = -1;
		if ( idStr ) {
			// Convert to long long 
			id = strtol( idStr, NULL, 10 );
			
			// Check user owns tute
			r = redisCommand( c, "SISMEMBER tute:tutor:%s %lld", user, id );
			if ( r->type != REDIS_REPLY_INTEGER )
			{
				header( out, 500, NULL );
				reason.str = "DB Error.";
				json_out( out, &failure );
				freeReplyObject( r );
				goto err;
			}
			if ( r->type != 1 )
			{
				header( out, 400, NULL );
				reason.str = "Invalid ID.";
				json_out( out, &failure );
				freeReplyObject( r );
				goto err;
			}
		} else {
			r = redisCommand( c, "INCR tute:tute:counter" );
			if ( r->type != REDIS_REPLY_INTEGER )
			{
				header( out, 500, NULL );
				reason.str = "Could not fetch tute counter.";
				json_out( out, &failure );
				freeReplyObject( r );
				goto err;
			}
			id = r->integer;
			freeReplyObject( r );
		}

		// Save tute
		r = redisCommand( c, "HMSET tute:tute:%lld tutor %s date %s start %s finish %s", id, user, date, start, finish );
		if ( r->type == REDIS_REPLY_ERROR )
		{
			header( out, 500, NULL );
			reason.str = "Could not save tute.";
			json_out( out, &failure );
			freeReplyObject( r );
			goto err;
		}
		freeReplyObject( r );

		// Save id to tutor
		r = redisCommand( c, "SADD tute:tutor:%s %lld", user, id );
		if ( r->type == REDIS_REPLY_ERROR )
		{
			header( out, 500, NULL );
			reason.str = "Could not save tute.";
			json_out( out, &failure );
			freeReplyObject( r );
			goto err;
		}
		freeReplyObject( r );

		// Parse codes
		param *course;
		ssize_t numCourses = convert_to_params( &course, courses, ";", ":", -1 ); 
		num = numCourses;
		while ( num-- )
		{
			// convert to lower
			to_lower( course[num].name );

			// Save each code to tute id
			r = redisCommand( c, "SADD tute:tute:%lld:courses %s", id, course[num].name );
			if ( r->type == REDIS_REPLY_ERROR )
			{
				header( out, 500, NULL );
				reason.str = "Could not save tute.";
				json_out( out, &failure );
				freeReplyObject( r );
				goto courseErr;
			}
			freeReplyObject( r );

			// Add tute id to each code
			r = redisCommand( c, "SADD tute:course:%s:tutes %lld", course[num].name, id );
			if ( r->type == REDIS_REPLY_ERROR )
			{
				header( out, 500, NULL );
				reason.str = "Could not save tute.";
				json_out( out, &failure );
				freeReplyObject( r );
				goto courseErr;
			}
			freeReplyObject( r );

		}
		
		// Parse students 
		param *student;
		num = convert_to_params( &student, students, ";", ":", -1 ); 
		while ( num-- )
		{
			// Add each student to tute
			r = redisCommand( c, "SADD tute:tute:%lld:students %s", id, student[num].name );
			if ( r->type == REDIS_REPLY_ERROR )
			{
				header( out, 500, NULL );
				reason.str = "Could not save tute.";
				json_out( out, &failure );
				freeReplyObject( r );
				goto studentErr;
			}
			freeReplyObject( r );

			// Save student data
			param *data;
			ssize_t fields = convert_to_params( &data, student[num].value, ",", "|", -1 );
			if ( fields ) { 

				char *college = NULL, *first = NULL, *last = NULL;	
				while ( fields-- )
				{
					if ( !college && !strcmp( data[fields].name, "college" ) ) {
						college = data[fields].value;
					} else if ( !first && !strcmp( data[fields].name, "first" ) ) {
						first = data[fields].value;
					} else if ( !last && !strcmp( data[fields].name, "last" ) ) {
						last = data[fields].value;
					}
				}
				
				if ( college && first && last )
				{
					r = redisCommand( c, "HMSET tute:student:%s college %s first %s last %s", student[num].name, college, first, last );
					if ( r->type == REDIS_REPLY_ERROR )
					{
						header( out, 500, NULL );
						reason.str = "Could not save tute.";
						json_out( out, &failure );
						freeReplyObject( r );
						free( data );
						goto studentErr;
					}
					freeReplyObject( r );
				}

				free( data );
			}

			// Add tute id to each student
			r = redisCommand( c, "SADD tute:attendee:%s %lld", student[num].name, id );
			if ( r->type == REDIS_REPLY_ERROR )
			{
				header( out, 500, NULL );
				reason.str = "Could not save tute.";
				json_out( out, &failure );
				freeReplyObject( r );
				goto studentErr;
			}
			freeReplyObject( r );

			// Add student to each course
			ssize_t n = numCourses;
			while ( n-- )
			{
				r = redisCommand( c, "SADD tute:course:%s:students %s", course[n].name, student[num].name );
				if ( r->type == REDIS_REPLY_ERROR )
				{
					header( out, 500, NULL );
					reason.str = "Could not save tute.";
					json_out( out, &failure );
					freeReplyObject( r );
					goto studentErr;
				}
				freeReplyObject( r );
			}

		}

		// Output success
		header( out, 200, sid );
		json_out( out, &success );
		
		studentErr:
			free( student ); // free params structure
		courseErr:
			free( course );
		err:
			free( user );
			free( p ); // Free params structure
			free( s ); // Free POST stream
	}
}

int main ( int argc, char *argv[] )
{
	return init( &process );
}
