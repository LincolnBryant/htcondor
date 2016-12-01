#include "condor_common.h"
#include "condor_config.h"
#include "subsystem_info.h"
#include "match_prefix.h"
#include "daemon.h"
#include "dc_annexd.h"
#include "stat_wrapper.h"
#include "condor_base64.h"

// Modelled on BulkRequest::validateAndStore() from annexd/BulkRequest.cpp.
bool
assignUserData( ClassAd & command, const char * ud, bool replace, std::string & validationError ) {
	ExprTree * launchConfigurationsTree = command.Lookup( "LaunchSpecifications" );
	if(! launchConfigurationsTree) {
		validationError = "Attribute 'LaunchSpecifications' missing.";
		return false;
	}
	classad::ExprList * launchConfigurations = dynamic_cast<classad::ExprList *>( launchConfigurationsTree );
	if(! launchConfigurations) {
		validationError = "Attribute 'LaunchSpecifications' not a list.";
		return false;
	}

	auto lcIterator = launchConfigurations->begin();
	for( ; lcIterator != launchConfigurations->end(); ++lcIterator ) {
		classad::ClassAd * ca = dynamic_cast<classad::ClassAd *>( * lcIterator );
		if( ca == NULL ) {
			validationError = "'LaunchSpecifications[x]' not a ClassAd.";
			return false;
		}
		ClassAd launchConfiguration( * ca );

		std::string userData;
		launchConfiguration.LookupString( "UserData", userData );
		if( userData.empty() || replace ) {
			ca->InsertAttr( "UserData", ud );
		}
	}

	return true;
}

// Modelled on readShortFile() from ec2_gahp/amazonCommands.cpp.
bool
readShortFile( const char * fileName, std::string & contents ) {
    int fd = safe_open_wrapper_follow( fileName, O_RDONLY, 0600 );

    if( fd < 0 ) {
    	fprintf( stderr, "Failed to open file '%s' for reading: '%s' (%d).\n",
            fileName, strerror( errno ), errno );
        return false;
    }

    StatWrapper sw( fd );
    unsigned long fileSize = sw.GetBuf()->st_size;

    char * rawBuffer = (char *)malloc( fileSize + 1 );
    assert( rawBuffer != NULL );
    unsigned long totalRead = full_read( fd, rawBuffer, fileSize );
    close( fd );
    if( totalRead != fileSize ) {
    	fprintf( stderr, "Failed to completely read file '%s'; needed %lu but got %lu.\n",
            fileName, fileSize, totalRead );
        free( rawBuffer );
        return false;
    }
    contents.assign( rawBuffer, fileSize );
    free( rawBuffer );

    return true;
}

void
help( const char * argv0 ) {
	// FIXME: Add a command-line flag the duration of the lease, duh.
	fprintf( stdout, "usage: %s\n"
		"\t[-public-key-file <public-key-file>]\n"
		"\t[-secret-key-file <secret-key-file>]\n"
		"\t[-pool <pool>] [-name <name>]\n"
		"\t[-service-url <service-url>] [-events-url <events-url>]\n"
		"\t[-lease-function-arn <lease-function-arn>]\n"
		"\t[-[default-]user-data[-file] <data|file> ]\n"
		"\t[-debug] [-help]\n"
		"\t<filename> <deadline>\n"
		, argv0 );
	fprintf( stdout, "where <filename> contains a JSON blob generated by "
		"the AWS web console (or one with an identical structure).\n\n" );
	fprintf( stdout, "\tHTCondor will create a one-shot Spot Fleet Request "
		"using the specified blob as a template.  The SFR will be of the "
		"(one-shot) request type, will not terminate its instances on "
		"expiration, and will be valid for just long enough to evaluate "
		"its bids (five minutes as of this writing).\n" );
	fprintf( stdout, "\tThe instances will be terminated at <deadline> "
		"by a lease mechanism which will also delete the SFR and itself "
		"at that time.\n" );
}

int
main( int argc, char ** argv ) {
	config();
	set_mySubSystem( "TOOL", SUBSYSTEM_TYPE_TOOL );

	bool clUserDataWins = true;
	std::string userData;
	const char * userDataFileName = NULL;

	bool quiet = false;
	int udSpecifications = 0;
	const char * pool = NULL;
	const char * name = NULL;
	const char * fileName = NULL;
	const char * serviceURL = NULL;
	const char * eventsURL = NULL;
	const char * publicKeyFile = NULL;
	const char * secretKeyFile = NULL;
	const char * leaseFunctionARN = NULL;
	long int leaseDuration = 0;
	for( int i = 1; i < argc; ++i ) {
		if( is_dash_arg_prefix( argv[i], "pool", 1 ) ) {
			++i;
			if( argv[i] != NULL ) {
				pool = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -pool requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "name", 1 ) ) {
			++i;
			if( argv[i] != NULL ) {
				name = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -name requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "service-url", 1 ) ) {
			++i;
			if( argv[i] != NULL ) {
				serviceURL = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -service-url requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "events-url", 1 ) ) {
			++i;
			if( argv[i] != NULL ) {
				eventsURL = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -events-url requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "user-data-file", 10 ) ) {
			++i; ++udSpecifications;
			if( argv[i] != NULL ) {
				userDataFileName = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -user-data-file requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "user-data", 9 ) ) {
			++i; ++udSpecifications;
			if( argv[i] != NULL ) {
				userData = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -user-data requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "default-user-data-file", 18 ) ) {
			++i; ++udSpecifications;
			if( argv[i] != NULL ) {
				clUserDataWins = false;
				userDataFileName = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -user-data-file requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "default-user-data", 17 ) ) {
			++i; ++udSpecifications;
			if( argv[i] != NULL ) {
				clUserDataWins = false;
				userData = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -user-data requires an argument.\n", argv[0] );
				return 1;
			}
		} else if( is_dash_arg_prefix( argv[i], "public-key-file", 1 ) ) {
			++i;
			if( argv[i] != NULL ) {
				publicKeyFile = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -public-key-file requires an argument.\n", argv[0] );
				return 1;
			}
			continue;
		} else if( is_dash_arg_prefix( argv[i], "secret-key-file", 1 ) ) {
			++i;
			if( argv[i] != NULL ) {
				secretKeyFile = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -secret-key-file requires an argument.\n", argv[0] );
				return 1;
			}
			continue;
		} else if( is_dash_arg_prefix( argv[i], "lease-function-arn", 1 ) ) {
			++i;
			if( argv[i] != NULL ) {
				leaseFunctionARN = argv[i];
				continue;
			} else {
				fprintf( stderr, "%s: -lease-function-arn requires an argument.\n", argv[0] );
				return 1;
			}
			continue;
		} else if( is_dash_arg_prefix( argv[i], "lease-duration", 1 ) ) {
			++i;
			if( argv[i] != NULL ) {
				char * endptr = NULL;
				const char * ld = argv[i];
				leaseDuration = strtol( ld, & endptr, 0 );
				if( * endptr != '\0' ) {
					fprintf( stderr, "%s: -lease-duration requires an integer argument.\n", argv[0] );
					return 1;
				}
				if( leaseDuration <= 0 ) {
					fprintf( stderr, "%s: the lease duration must be greater than zero.\n", argv[0] );
					return 1;
				}
				continue;
			} else {
				fprintf( stderr, "%s: -lease-duration requires an argument.\n", argv[0] );
				return 1;
			}
			continue;
		} else if( is_dash_arg_prefix( argv[i], "debug", 1 ) ) {
			dprintf_set_tool_debug( "TOOL", 0 );
			continue;
		} else if( is_dash_arg_prefix( argv[i], "help", 1 ) ) {
			help( argv[0] );
			return 1;
		} else if( argv[i][0] == '-' && argv[i][1] != '\0' ) {
			fprintf( stderr, "%s: unrecognized option (%s).\n", argv[0], argv[i] );
			return 1;
		} else {
			fileName = argv[i];
			continue;
		}
	}

	if( udSpecifications > 1 ) {
		fprintf( stderr, "%s: you may specify no more than one of -[default-]user-data[-file].\n", argv[0] );
		return 1;
	}

	if( fileName == NULL ) {
		fprintf( stderr, "%s: you must specify a file containing an annex specification.\n", argv[0] );
		return 1;
	}

	// FIXME: Switch from -lease-duration to a positional argument, where the
	// positional argument accepts '+<number>' as "<number> of seconds from
	// now."
	if( leaseDuration == 0 ) {
		fprintf( stderr, "%s: you must specify -lease-duration.\n", argv[0] );
		return 1;
	}

	FILE * file = NULL;
	bool closeFile = true;
	if( strcmp( fileName, "-" ) == 0 ) {
		file = stdin;
		closeFile = false;
	} else {
		file = safe_fopen_wrapper_follow( fileName, "r" );
		if( file == NULL ) {
			fprintf( stderr, "Unable to open annex specification file '%s'.\n", fileName );
			return 1;
		}
	}


	std::string annexDaemonName;
	param( annexDaemonName, "ANNEXD_NAME" );
	if( name == NULL ) { name = annexDaemonName.c_str(); }
	DCAnnexd annexd( name, pool );
	annexd.setSubsystem( "GENERIC" );


	ClassAd spotFleetRequest;
	CondorClassAdFileIterator ccafi;
	if(! ccafi.begin( file, closeFile, CondorClassAdFileParseHelper::Parse_json )) {
		fprintf( stderr, "Failed to start parsing spot fleet request.\n" );
		return 2;
	} else {
		int numAttrs = ccafi.next( spotFleetRequest );
		if( numAttrs <= 0 ) {
			fprintf( stderr, "Failed to parse spot fleet request, found no attributes.\n" );
			return 2;
		} else if( numAttrs > 11 ) {
			fprintf( stderr, "Failed to parse spot fleet reqeust, found too many attributes.\n" );
			return 2;
		}
	}

	time_t now = time( NULL );
	spotFleetRequest.Assign( "EndOfLease", now + leaseDuration );

	if( serviceURL != NULL ) {
		spotFleetRequest.Assign( "ServiceURL", serviceURL );
	}

	if( eventsURL != NULL ) {
		spotFleetRequest.Assign( "EventsURL", eventsURL );
	}

	if( publicKeyFile != NULL ) {
		spotFleetRequest.Assign( "PublicKeyFile", publicKeyFile );
	}

	if( secretKeyFile != NULL ) {
		spotFleetRequest.Assign( "SecretKeyFile", secretKeyFile );
	}

	if( leaseFunctionARN != NULL ) {
		spotFleetRequest.Assign( "LeaseFunctionARN", leaseFunctionARN );
	}

	// Handle user data.
	if( userDataFileName != NULL ) {
		if(! readShortFile( userDataFileName, userData ) ) {
			return 2;
		}
	}

	// condor_base64_encode() segfaults on the empty string.
	if(! userData.empty()) {
		std::string validationError;
		char * base64Encoded = condor_base64_encode( (const unsigned char *)userData.c_str(), userData.length() );
		if(! assignUserData( spotFleetRequest, base64Encoded, clUserDataWins, validationError )) {
			fprintf( stderr, "Failed to set user data in request ad (%s).\n", validationError.c_str() );
			return 6;
		}
		free( base64Encoded );
	}


	if(! annexd.locate( Daemon::LOCATE_FOR_LOOKUP )) {
		char * error = annexd.error();
		if( error && error[0] != '\0' ) {
			fprintf( stderr, "%s: Can't locate annex daemon (%s).\n", argv[0], error );
		} else {
			fprintf( stderr, "%s: Can't locate annex daemon.\n", argv[0] );
		}
		return 3;
	}


	ClassAd reply;
	if(! quiet) { fprintf( stdout, "Sending bulk request command to daemon...\n" ); }
	if(! annexd.sendBulkRequest( & spotFleetRequest, & reply )) {
		char * error = annexd.error();
		if( error && error[0] != '\0' ) {
			fprintf( stderr, "%s\n", error );
		} else {
			fprintf( stderr, "Failed to send bulk request to daemon.\n" );
		}
		return 4;
	}

	int requestVersion = -1;
	reply.LookupInteger( "RequestVersion", requestVersion );
	if( requestVersion != 1 ) {
		fprintf( stderr, "Daemon's reply had missing or unknown RequestVersion (%d).\n", requestVersion );
		return 5;
	}

	std::string bulkRequestID;
	reply.LookupString( "BulkRequestID", bulkRequestID );
	if( bulkRequestID.empty() ) {
		fprintf( stderr, "Daemon's reply did not include bulk request ID.\n" );
		return 5;
	} else {
		fprintf( stdout, "%s\n", bulkRequestID.c_str() );
	}

	return 0;
}
