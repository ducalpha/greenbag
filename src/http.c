// Copyright 2017 Duc Hoang Bui, KAIST. All rights reserved.
// Licensed under MIT (https://github.com/ducalpha/greenbag/blob/master/LICENSE)

/* HTTP control file							*/
#include "config.h"
#include "gbsession.h"
#include "gb.h"
#include "util.h"
// set http_t
int http_connect( http_t *conn, char *host, int port, char *user, char *pass )
{
	char base64_encode[64] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz0123456789+/";
	char auth[MAX_STRING];
	int i;
	strncpy( conn->host, host, MAX_STRING );
	if( ( conn->fd = tcp_connect( host, port, conn->local_if ) ) == -1 )
	{
		/* We'll put the message in conn->headers, not in request */
		//sprintf( conn->headers, _("Unable to connect to server %s:%i\n"), host, port );
		fprintf( stderr, "Unable to connect to server %s:%i\n", host, port );
		return( 0 );
	}
	
	if( *user == 0 )
	{
		*conn->auth = 0;
	}
	else
	{
		memset( auth, 0, MAX_STRING );
		snprintf( auth, MAX_STRING, "%s:%s", user, pass );
		for( i = 0; auth[i*3]; i ++ )
		{
			conn->auth[i*4] = base64_encode[(auth[i*3]>>2)];
			conn->auth[i*4+1] = base64_encode[((auth[i*3]&3)<<4)|(auth[i*3+1]>>4)];
			conn->auth[i*4+2] = base64_encode[((auth[i*3+1]&15)<<2)|(auth[i*3+2]>>6)];
			conn->auth[i*4+3] = base64_encode[auth[i*3+2]&63];
			if( auth[i*3+2] == 0 ) conn->auth[i*4+3] = '=';
			if( auth[i*3+1] == 0 ) conn->auth[i*4+2] = '=';
		}
	}
	
	return( 1 );
}

void http_disconnect( http_t *conn )
{
	if( conn->fd > 0 )
		close( conn->fd );
	conn->fd = -1;
}
/*
 * create basic GET request to conn->request
 * conn is of type http_t
 */
void http_get( http_t *http_conn, char *lurl )
{
	*http_conn->request = 0;
	
	http_addheader( http_conn, "GET %s HTTP/1.1", lurl );
	http_addheader( http_conn, "Host: %s", http_conn->host );

	if( *http_conn->auth )
		http_addheader( http_conn, "Authorization: Basic %s", http_conn->auth );
	
	if( http_conn->lastbyte )
		http_addheader( http_conn, "Range: bytes=%lld-%lld", http_conn->firstbyte, http_conn->lastbyte );
	else
		http_addheader( http_conn, "Range: bytes=%lld-", http_conn->firstbyte );
}

/* add a header HTTP request message
 * to HTTP conn->request
 */
void http_addheader( http_t *http_conn, char *format, ... )
{
	char s[MAX_STRING];
	va_list params;
	
	va_start( params, format );
	vsnprintf( s, MAX_STRING - 3, format, params );
	strcat( s, "\r\n" );
	va_end( params );
	strncat( http_conn->request, s, MAX_QUERY - strlen(http_conn->request) - 1);
}
/* send HTTP request and receive HTTP response headers */
int http_exec( http_t *http_conn )
{
	int i = 0;
	char s[2] = " ", *s2;
#ifdef DEBUG
	fprintf( stderr, "--- Sending request ---\n%s--- End of request ---\n", http_conn->request );
#endif
	http_addheader(http_conn, "Connection: keep-alive");
	http_addheader( http_conn, "" );
  //may use socket read timeout:
  //setsockopt(http_conn->fd, SOL_SOCKET, SO_RCVTIMEOUT, (char *)&tv, sizeof(struct timeval));
	write( http_conn->fd, http_conn->request, strlen( http_conn->request ) );
	*http_conn->headers = 0;
	/* Read the response headers byte by byte to make sure we don't touch the actual data	*/
	while (1) // pthread_cancel likely breaks this loop
  //while(gb && (gb->ready == 0) && gb->run) //quit here!
	{
		if( read( http_conn->fd, s, 1 ) <= 0 ) // may block here?
		{
			/* We'll put the message in http_conn->headers, not in request */
			sprintf( http_conn->headers, _("Connection gone.\n") );
			return( 0 );
		}
		if( *s == '\r' )
		{
			continue;
		}
		else if( *s == '\n' )
		{
			if( i == 0 )
				break;
			i = 0;
		}
		else
		{
			i ++;
		}
	//printf("### %s\n", http_conn->headers);
		strncat( http_conn->headers, s, MAX_QUERY );
	}
#ifdef DEBUG
	fprintf( stderr, "--- Reply headers ---\n%s--- End of headers ---\n", http_conn->headers );
#endif
	sscanf( http_conn->headers, "%*s %3i", &http_conn->status );
	s2 = strchr( http_conn->headers, '\n' ); *s2 = 0;
	strcpy( http_conn->request, http_conn->headers );//TODO why this? HTTP request <-- HTTP response
	*s2 = '\n';
	return( 1 );
}

char *http_header( http_t *http_conn, char *header )
{
	char s[32];
	int i;
	
	for( i = 1; http_conn->headers[i]; i ++ )
		if( http_conn->headers[i-1] == '\n' )
		{
			sscanf( &http_conn->headers[i], "%31s", s );
			if( strcasecmp( s, header ) == 0 )
				return( &http_conn->headers[i+strlen(header)] );
		}
	
	return( NULL );
}

long long int http_size( http_t *http_conn )
{
	char *i;
	long long int j;
	
	if( ( i = http_header( http_conn, "Content-Length:" ) ) == NULL )
		return( -2 );
	
	sscanf( i, "%lld", &j );
	return( j );
}

/* Decode%20a%20file%20name						*/
void http_decode( char *s )
{
	char t[MAX_STRING];
	int i, j, k;
	
	for( i = j = 0; s[i]; i ++, j ++ )
	{
		t[j] = s[i];
		if( s[i] == '%' )
			if( sscanf( s + i + 1, "%2x", &k ) )
			{
				t[j] = k;
				i += 2;
			}
	}
	t[j] = 0;
	
	strcpy( s, t );
}

void http_encode( char *s )
{
	char t[MAX_STRING];
	int i, j;
	
	for( i = j = 0; s[i]; i ++, j ++ )
	{
		/* Fix buffer overflow */
		if (j >= MAX_STRING - 1) {
			break;
		}
		
		t[j] = s[i];
		if( s[i] == ' ' )
		{
			/* Fix buffer overflow */
			if (j >= MAX_STRING - 3) {
				break;
			}
			
			strcpy( t + j, "%20" );
			j += 2;
		}
	}
	t[j] = 0;
	
	strcpy( s, t );
}
