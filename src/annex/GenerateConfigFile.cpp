#include "condor_common.h"
#include "classad_collection.h"
#include "gahp-client.h"
#include "Functor.h"
#include "GenerateConfigFile.h"

#include "condor_config.h"
#include "filename_tools.h"
#include "directory.h"
#include "safe_fopen.h"

int
GenerateConfigFile::operator() () {
	dprintf( D_FULLDEBUG, "GenerateConfigFile::operator()\n" );

	std::map< std::string, std::string > mapping;
	mapping[ "S3BucketName" ] = "ANNEX_DEFAULT_S3_BUCKET";
	mapping[ "odiLeaseFunctionARN" ] = "ANNEX_DEFAULT_ODI_LEASE_FUNCTION_ARN";
	mapping[ "sfrLeaseFunctionARN" ] = "ANNEX_DEFAULT_SFR_LEASE_FUNCTION_ARN";
	mapping[ "InstanceProfileARN" ] = "ANNEX_DEFAULT_ODI_INSTANCE_PROFILE_ARN";
	mapping[ "SecurityGroupID" ] = "ANNEX_DEFAULT_ODI_SECURITY_GROUP_IDS";
	mapping[ "KeyName" ] = "ANNEX_DEFAULT_ODI_KEY_NAME";
	mapping[ "AccessKeyFile" ] = "ANNEX_DEFAULT_ACCESS_KEY_FILE";
	mapping[ "SecretKeyFile" ] = "ANNEX_DEFAULT_SECRET_KEY_FILE";

	// Append the annex configuration to the user config file.
	FILE * configFile = NULL;

	// Consider using createUserConfigDir() from CreateKeyPair.cpp.
	std::string userConfigName;
	MyString userConfigSource;
	param( userConfigName, "USER_CONFIG_FILE" );
	if(! userConfigName.empty()) {
		find_user_file( userConfigSource, userConfigName.c_str(), false );
		if(! userConfigSource.empty()) {
			// Create the containing directory if necessary, and only the
			// containing directory -- don't do anything stupid if the
			// user configuration directory is misconfigured.
			std::string dir, file;
			filename_split( userConfigSource.c_str(), dir, file );
			if(! IsDirectory( dir.c_str() )) {
				mkdir( dir.c_str(), 0755 );
			}

			configFile = safe_fcreate_keep_if_exists_follow( userConfigSource.c_str(),
				"a", 0644 );
			if( configFile == NULL ) {
				fprintf( stderr, "Failed to open user configuration file '%s': %s (%d).  Printing configuration...\n",
					userConfigSource.c_str(), strerror( errno ), errno );
				configFile = stdout;
			}
		} else {
			fprintf( stderr, "Unable to locate your user configuration file.  Printing configuration...\n" );
			configFile = stdout;
		}
	} else {
		fprintf( stderr, "Your HTCondor installation is configured to ignore user configuration files.  Contact your system administrator.  Printing configuration...\n" );
		configFile = stdout;
	}

	fprintf( configFile, "\n" );
	fprintf( configFile, "# Generated by condor_annex -setup.\n" );

	std::string value;
	for( auto i = mapping.begin(); i != mapping.end(); ++i ) {
		value.clear();
		scratchpad->LookupString( i->first.c_str(), value );
		if(! value.empty()) {
			fprintf( configFile, "%s = %s\n", i->second.c_str(), value.c_str() );
		}
	}

	std::string keyPath;
	scratchpad->LookupString( "KeyPath", keyPath );
	if(! keyPath.empty()) {
		fprintf( configFile, "# For debugging:\n" );
		fprintf( configFile, "# ssh -i %s ec2-user@<address>\n", keyPath.c_str() );
	}

	fprintf( configFile, "\n" );

	daemonCore->Reset_Timer( gahp->getNotificationTimerId(), 0, TIMER_NEVER );
	return PASS_STREAM;
}

int
GenerateConfigFile::rollback() {
	dprintf( D_FULLDEBUG, "GenerateConfigFile::rollback()\n" );

	// This functor does nothing (to the service), so don't undo anything.

	daemonCore->Reset_Timer( gahp->getNotificationTimerId(), 0, TIMER_NEVER );
	return PASS_STREAM;
}
