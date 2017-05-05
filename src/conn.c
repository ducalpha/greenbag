// Copyright 2017 Duc Hoang Bui, KAIST. All rights reserved.
// Licensed under MIT (https://github.com/ducalpha/greenbag/blob/master/LICENSE)

/* Connection stuff							*/
#include "gb.h"
char string[MAX_STRING];
/* Parse an URL to a conn_t structure					*/
// return 0 indicates error
int conn_set( conn_t *conn, char *set_url )
{
	char url[MAX_STRING];
	char *i, *j;
	
	/* protocol://							*/
	if( ( i = strstr( set_url, "://" ) ) == NULL )
	{
		conn->proto = PROTO_DEFAULT;
		strncpy( url, set_url, MAX_STRING );
	}
	else
	{
		if( set_url[0] == 'h' )
			conn->proto = PROTO_HTTP;
		else
		{
			return( 0 );
		}
		strncpy( url, i + 3, MAX_STRING );
	}
	/* Split							*/
	if( ( i = strchr( url, '/' ) ) == NULL )
	{
		strcpy( conn->dir, "/" );
	}
	else
	{
		*i = 0;
		snprintf( conn->dir, MAX_STRING, "/%s", i + 1 );
		if( conn->proto == PROTO_HTTP )
			http_encode( conn->dir );
	}
	strncpy( conn->host, url, MAX_STRING );
	j = strchr( conn->dir, '?' );
	if( j != NULL )
		*j = 0;
	i = strrchr( conn->dir, '/' );
	*i = 0;
	if( j != NULL )
		*j = '?';
	if( i == NULL )
	{
		strncpy( conn->file, conn->dir, MAX_STRING );
		strcpy( conn->dir, "/" );
	}
	else
	{
		strncpy( conn->file, i + 1, MAX_STRING );
		strcat( conn->dir, "/" );
	}
	
	/* Check for username in host field				*/
	if( strrchr( conn->host, '@' ) != NULL )
	{
		strncpy( conn->user, conn->host, MAX_STRING );
		i = strrchr( conn->user, '@' );
		*i = 0;
		strncpy( conn->host, i + 1, MAX_STRING );
		*conn->pass = 0;
	}
	/* If not: Fill in defaults					*/
	else
	{
		if( conn->proto == PROTO_HTTP )
		{
			*conn->user = *conn->pass = 0;
		}
	}
	
	/* Password?							*/
	if( ( i = strchr( conn->user, ':' ) ) != NULL )
	{
		*i = 0;
		strncpy( conn->pass, i + 1, MAX_STRING );
	}
	/* Port number?							*/
	if( ( i = strchr( conn->host, ':' ) ) != NULL )
	{
		*i = 0;
		sscanf( i + 1, "%i", &conn->port );
	}
	/* Take default port numbers from /etc/services			*/
	else
	{
/*get port number for service name
#ifndef DARWIN
		struct servent *serv;
		
		if( conn->proto == PROTO_FTP )
			serv = getservbyname( "ftp", "tcp" );
		else
			serv = getservbyname( "www", "tcp" );
		
		if( serv )
			conn->port = ntohs( serv->s_port );
		else
#endif*/
		if( conn->proto == PROTO_HTTP )
			conn->port = 80;
	}
	
  printf("conn->host: %s\tconn->port: %d\tconn->dir: %s\tconn->file: %s\n", conn->host, conn->port, conn->dir, conn->file);
  //e.g. conn->host: 10.0.0.2     conn->port: 80  conn->dir: /ducbh/files/        conn->file: TurkishAirlines.mp4

	return( conn->port > 0 );
}

/* Generate a nice URL string.						*/
// e.g. http://ducalpha:1234@cps.kaist.ac.kr/ducbh/file
char *conn_url( conn_t *conn )
{
	if( conn->proto == PROTO_HTTP )
		strcpy( string, "http://" );
	if( *conn->user != 0 && strcmp( conn->user, "anonymous" ) != 0 )
		sprintf( string + strlen( string ), "%s:%s@",
			conn->user, conn->pass );
	sprintf( string + strlen( string ), "%s:%i%s%s",
		conn->host, conn->port, conn->dir, conn->file );
	return( string );
}

/* Close connection */
void conn_disconnect( conn_t *conn )
{
	if (conn->proto == PROTO_HTTP)
		http_disconnect( conn->http );
	conn->fd = -1;
}

int conn_init( conn_t *conn )
{
	if( conn->proto == PROTO_HTTP )
	{
		conn->http->local_if = conn->local_if;
		if( !http_connect( conn->http, conn->host, conn->port, conn->user, conn->pass ) )
		{
			conn->message = conn->http->headers;
			conn_disconnect( conn );
			return( 0 );
		}
		conn->message = conn->http->headers;
		conn->fd = conn->http->fd;
	}
	return( 1 );
}
/*
 * construct HTTP request message
 */
int conn_setup( conn_t *conn )
{
	if( conn->http->fd <= 0 )
		if( !conn_init( conn ) )
			return( 0 );
	if( conn->proto == PROTO_HTTP)
	{
		char s[MAX_STRING];
		int i;

		snprintf( s, MAX_STRING, "%s%s", conn->dir, conn->file );
		conn->http->firstbyte = conn->currentbyte;
		conn->http->lastbyte = conn->lastbyte;
		http_get(conn->http, s);
		http_addheader(conn->http, "User-Agent: %s", conn->conf->user_agent);
		for( i = 0; i < conn->conf->add_header_count; i++)
			http_addheader( conn->http, "%s", conn->conf->add_header[i] );
	}
	return( 1 );
}

int conn_exec( conn_t *conn )
{
	if( conn->proto == PROTO_HTTP )
	{
		if( !http_exec( conn->http ) )
			return( 0 );
		return( conn->http->status / 100 == 2 );
	}
  return 0;
}

/* Get file size and other information					*/
int conn_info( conn_t *conn )
{
	/* It's all a bit messed up.. But it works.			*/
	if( conn->proto == PROTO_HTTP)
	{
		char s[MAX_STRING], *t;
		long long int i = 0;
		
		do
		{
			conn->currentbyte = 1;
			if( !conn_setup( conn ) )
				return( 0 );
			conn_exec( conn );
			conn_disconnect( conn );

			/* Code 3xx == redirect				*/
			if( conn->http->status / 100 != 3 )
				break;
			if( ( t = http_header( conn->http, "location:" ) ) == NULL )
				return( 0 );
			sscanf( t, "%255s", s );
			if( strstr( s, "://" ) == NULL)
			{
				sprintf( conn->http->headers, "%s%s",
					conn_url( conn ), s );
				strncpy( s, conn->http->headers, MAX_STRING );
			}
			else if( s[0] == '/' )
			{
				sprintf( conn->http->headers, "http://%s:%i%s",
					conn->host, conn->port, s );
				strncpy( s, conn->http->headers, MAX_STRING );
			}
			conn_set( conn, s );
			i ++;
		} while( conn->http->status / 100 == 3 && i < MAX_REDIR );
		
		if( i == MAX_REDIR )
		{
			sprintf( conn->message, _("Too many redirects.\n") );
			return( 0 );
		}
		
		conn->size = http_size( conn->http );
		if( conn->http->status == 206 && conn->size >= 0 )
		{
			conn->supported = 1;
			conn->size ++;
		}
		else if( conn->http->status == 200 || conn->http->status == 206 )
		{
			conn->supported = 0;
			conn->size = INT_MAX;
		}
		else
		{
			t = strchr( conn->message, '\n' );
			if( t == NULL )
				sprintf( conn->message, _("Unknown HTTP error.\n") );
			else
				*t = 0;
			return( 0 );
		}
	}
	
	return( 1 );
}
