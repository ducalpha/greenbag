// Copyright 2017 Duc Hoang Bui, KAIST. All rights reserved.
// Licensed under MIT (https://github.com/ducalpha/greenbag/blob/master/LICENSE)

#include "config.h"
#include "gb.h"
#include "util.h"
/* Some nifty macro's..							*/
#define get_config_string( name )				\
	if( strcmp( key, #name ) == 0 )				\
	{							\
		st = 1;						\
		strcpy( conf->name, value );			\
	}
#define get_config_number( name )				\
	if( strcmp( key, #name ) == 0 )				\
	{							\
		st = 1;						\
		sscanf( value, "%i", &conf->name );		\
	}
	
int parse_interfaces( conf_t *conf, char *s );

int conf_loadfile( conf_t *conf, char *file )
{
	int i, line = 0;
	FILE *fp;
	char s[MAX_STRING], key[MAX_STRING], value[MAX_STRING];
	
	fp = fopen( file, "r" );
	if( fp == NULL )
		return( 1 );			/* Not a real failure	*/
	
	while( !feof( fp ) )
	{
		int st;
		
		line ++;
		
		*s = 0;
		fscanf( fp, "%100[^\n#]s", s );
		fscanf( fp, "%*[^\n]s" );
		fgetc( fp );			/* Skip newline		*/
		if( strchr( s, '=' ) == NULL )
			continue;		/* Probably empty?	*/
		sscanf( s, "%[^= \t]s", key );
		for( i = 0; s[i]; i ++ )
			if( s[i] == '=' )
			{
				for( i ++; isspace( (int) s[i] ) && s[i]; i ++ );
				break;
			}
		strcpy( value, &s[i] );
		for( i = strlen( value ) - 1; isspace( (int) value[i] ); i -- )
			value[i] = 0;
		
		st = 0;
		
		/* Long live macros!!					*/
		get_config_string( default_filename );
		get_config_number( strip_cgi_parameters );
		get_config_number( connection_timeout );
		get_config_number( reconnect_delay );
		get_config_number( num_connections );
		get_config_number( buffer_size );
		get_config_number( verbose );
		get_config_number( alternate_output );
		
		get_config_number( search_timeout );
		get_config_number( search_threads );
		get_config_number( search_amount );
		get_config_number( search_top );
		
		/* Option defunct but shouldn't be an error		*/
		if( strcmp( key, "speed_type" ) == 0 )
			st = 1;
		
		if( strcmp( key, "interfaces" ) == 0 )
			st = parse_interfaces( conf, value );
		
		if( !st )
		{
			fprintf( stderr, _("Error in %s line %i.\n"), file, line );
			return( 0 );
		}
		get_config_number( add_header_count );
		for(i=0;i<conf->add_header_count;i++)
			get_config_string( add_header[i] );
		get_config_string( user_agent );
	}
	
	fclose( fp );
	return( 1 );
}

int conf_init( conf_t *conf )
{
	char s[MAX_STRING], *s2;
	
	/* Set defaults							*/
	memset( conf, 0, sizeof( conf_t ) );
	strcpy( conf->default_filename, "default" );
	conf->strip_cgi_parameters	= 1;
	conf->connection_timeout	= 45;
	conf->reconnect_delay		= 20;
  conf->num_connections = NUM_CONNECTIONS;
	conf->buffer_size		= 5120;
	conf->verbose			= VERBOSITY;
	conf->alternate_output		= 1;
//sangeun
	conf->num_of_segments	= 0;
	conf->initial_chunk_rate[0]	= 1;
	conf->initial_chunk_rate[1] = 1;
	conf->qos_kbps	= 6000;
	conf->fixed_segment_mode = 0;
	conf->fixed_subsegment_mode = 0;
	conf->minimum_energy_mode = 0;
	conf->ewma_factor = 0.3;
	conf->BW_factor_for_mode = 1;
	conf->BW_factor_for_stage = 1;
	conf->fixed_segment_size_KB = 10240; // 1MB
	conf->recovery_mode = 0;
	conf->recovery_decision_factor = 1.0;
	strcpy(conf->log_file, "log.txt");
//sangeun
	
	conf->search_timeout		= 10;
	conf->search_threads		= 3;
	conf->search_amount		= 15;
	conf->search_top		= 3;
	conf->add_header_count		= 0;
	strncpy( conf->user_agent, DEFAULT_USER_AGENT, MAX_STRING );
	
	conf->interfaces = malloc( sizeof( if_t ) );
	memset( conf->interfaces, 0, sizeof( if_t ) );
	conf->interfaces->next = conf->interfaces;
	
	
	if( !conf_loadfile( conf, ETCDIR "/gbrc" ) )
		return( 0 );
	
	if( ( s2 = getenv( "HOME" ) ) != NULL )
	{
		sprintf( s, "%s/%s", s2, ".gbrc" );
		if( !conf_loadfile( conf, s ) )
			return( 0 );
	}
	
	return( 1 );
}

int parse_interfaces( conf_t *conf, char *s )
{
	char *s2;
	if_t *iface;
	
	iface = conf->interfaces->next;
	while( iface != conf->interfaces )
	{
		if_t *i;
		
		i = iface->next;
		free( iface );
		iface = i;
	}
	free( conf->interfaces );
	
	if( !*s )
	{
		conf->interfaces = malloc( sizeof( if_t ) );
		memset( conf->interfaces, 0, sizeof( if_t ) );
		conf->interfaces->next = conf->interfaces;
		return( 1 );
	}
	
	s[strlen(s)+1] = 0;
	conf->interfaces = iface = malloc( sizeof( if_t ) );
	while( 1 )
	{
		while( ( *s == ' ' || *s == '\t' ) && *s ) s ++;
		for( s2 = s; *s2 != ' ' && *s2 != '\t' && *s2; s2 ++ );
		*s2 = 0;
		if( *s < '0' || *s > '9' )
			get_if_ip( s, iface->text );
		else
			strcpy( iface->text, s );
		s = s2 + 1;
		if( *s )
		{
			iface->next = malloc( sizeof( if_t ) );
			iface = iface->next;
		}
		else
		{
			iface->next = conf->interfaces;
			break;
		}
	}
	
	return( 1 );
}
