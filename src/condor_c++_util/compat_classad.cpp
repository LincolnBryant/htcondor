/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#include "compat_classad.h"

#include "condor_classad.h"
#include "classad_oldnew.h"
#include "condor_attributes.h"
#include "classad/classad/xmlSink.h"
#include "condor_xml_classads.h"
#include "condor_config.h"

using namespace std;

namespace compat_classad {

// EvalResult ctor
EvalResult::EvalResult()
{
	type = LX_UNDEFINED;
	debug = false;
}

// EvalResult dtor
EvalResult::~EvalResult()
{
	if ((type == LX_STRING || type == LX_TIME) && (s)) {
		delete [] s;
	}
}

void
EvalResult::deepcopy(const EvalResult & rhs)
{
	type = rhs.type;
	debug = rhs.debug;
	switch ( type ) {
		case LX_INTEGER:
		case LX_BOOL:
			i = rhs.i;
			break;
		case LX_FLOAT:
			f = rhs.f;
			break;
		case LX_STRING:
				// need to make a deep copy of the string
			s = strnewp( rhs.s );
			break;
		default:
			break;
	}
}

// EvalResult copy ctor
EvalResult::EvalResult(const EvalResult & rhs)
{
	deepcopy(rhs);
}

// EvalResult assignment op
EvalResult & EvalResult::operator=(const EvalResult & rhs)
{
	if ( this == &rhs )	{	// object assigned to itself
		return *this;		// all done.
	}

		// deallocate any state in this object by invoking dtor
	this->~EvalResult();

		// call copy ctor to make a deep copy of data
	deepcopy(rhs);

		// return reference to invoking object
	return *this;
}


void EvalResult::fPrintResult(FILE *fi)
{
    switch(type)
    {
	case LX_INTEGER :

	     fprintf(fi, "%d", this->i);
	     break;

	case LX_FLOAT :

	     fprintf(fi, "%f", this->f);
	     break;

	case LX_STRING :

	     fprintf(fi, "%s", this->s);
	     break;

	case LX_NULL :

	     fprintf(fi, "NULL");
	     break;

	case LX_UNDEFINED :

	     fprintf(fi, "UNDEFINED");
	     break;

	case LX_ERROR :

	     fprintf(fi, "ERROR");
	     break;

	default :

	     fprintf(fi, "type unknown");
	     break;
    }
    fprintf(fi, "\n");
}

void EvalResult::toString(bool force)
{
	switch(type) {
		case LX_STRING:
			break;
		case LX_FLOAT: {
			MyString buf;
			buf.sprintf("%lf",f);
			s = strnewp(buf.Value());
			type = LX_STRING;
			break;
		}
		case LX_BOOL:	
			type = LX_STRING;
			if (i) {
				s = strnewp("TRUE");
			} else {
				s = strnewp("FALSE");
			}	
			break;
		case LX_INTEGER: {
			MyString buf;
			buf.sprintf("%d",i);
			s = strnewp(buf.Value());
			type = LX_STRING;
			break;
		}
		case LX_UNDEFINED:
			if( force ) {
				s = strnewp("UNDEFINED");
				type = LX_STRING;
			}
			break;
		case LX_ERROR:
			if( force ) {
				s = strnewp("ERROR");
				type = LX_STRING;
			}
			break;
		default:
			ASSERT("Unknown classad result type");
	}
}

ClassAd::ClassAd()
{
	m_privateAttrsAreInvisible = false;

		// Compatibility ads are born with this to emulate the special
		// CurrentTime in old ClassAds. We don't protect it afterwards,
		// but that shouldn't be problem unless someone is deliberately
		// trying to shoot themselves in the foot.
	AssignExpr( ATTR_CURRENT_TIME, "time()" );

	ResetName();
    ResetExpr();

	EnableDirtyTracking();
}

ClassAd::ClassAd( const ClassAd &ad )
{
	CopyFrom( ad );

		// Compatibility ads are born with this to emulate the special
		// CurrentTime in old ClassAds. We don't protect it afterwards,
		// but that shouldn't be problem unless someone is deliberately
		// trying to shoot themselves in the foot.
	AssignExpr( ATTR_CURRENT_TIME, "time()" );

	m_privateAttrsAreInvisible = false;

	ResetName();
    ResetExpr();

	EnableDirtyTracking();
}

ClassAd::ClassAd( const classad::ClassAd &ad )
{
	CopyFrom( ad );

		// Compatibility ads are born with this to emulate the special
		// CurrentTime in old ClassAds. We don't protect it afterwards,
		// but that shouldn't be problem unless someone is deliberately
		// trying to shoot themselves in the foot.
	AssignExpr( ATTR_CURRENT_TIME, "time()" );

	m_privateAttrsAreInvisible = false;

	ResetName();
    ResetExpr();

	EnableDirtyTracking();
}

ClassAd::~ClassAd()
{
}

ClassAd::
ClassAd( FILE *file, char *delimitor, int &isEOF, int&error, int &empty )
{
	m_privateAttrsAreInvisible = false;

	nodeKind = CLASSAD_NODE;

	char		buffer[ATTRLIST_MAX_EXPRESSION];
	int			delimLen = strlen( delimitor );

	buffer[0] = '\0';
	while( 1 ) {

			// get a line from the file
		if( fgets( buffer, delimLen+1, file ) == NULL ) {
			error = ( isEOF = feof( file ) ) ? 0 : errno;
			return;
		}

			// did we hit the delimitor?
		if( strncmp( buffer, delimitor, delimLen ) == 0 ) {
				// yes ... stop
			isEOF = feof( file );
			error = 0;
			return;
		} else {
				// no ... read the rest of the line (into the same buffer)
			if( fgets( buffer+delimLen, ATTRLIST_MAX_EXPRESSION-delimLen,file )
					== NULL ) {
				error = ( isEOF = feof( file ) ) ? 0 : errno;
				return;
			}
		}

			// if the string is empty, try reading again
		if( strlen( buffer ) == 0 || strcmp( buffer, "\n" ) == 0 ) {
			continue;
		}

			// Insert the string into the classad
		if( Insert( buffer ) == FALSE ) { 	
				// print out where we barfed to the log file
			dprintf(D_ALWAYS,"failed to create classad; bad expr = %s\n",
				buffer);
				// read until delimitor or EOF; whichever comes first
			while( strncmp( buffer, delimitor, delimLen ) && !feof( file ) ) {
				fgets( buffer, delimLen+1, file );
			}
			isEOF = feof( file );
			error = -1;
			return;
		} else {
			empty = FALSE;
		}
	}

		// Compatibility ads are born with this to emulate the special
		// CurrentTime in old ClassAds. We don't protect it afterwards,
		// but that shouldn't be problem unless someone is deliberately
		// trying to shoot themselves in the foot.
	AssignExpr( ATTR_CURRENT_TIME, "time()" );

	ResetName();
    ResetExpr();

	EnableDirtyTracking();
}

bool ClassAd::
ClassAdAttributeIsPrivate( char const *name )
{
	if( stricmp(name,ATTR_CLAIM_ID) == 0 ) {
			// This attribute contains the secret capability cookie
		return true;
	}
	if( stricmp(name,ATTR_CAPABILITY) == 0 ) {
			// This attribute contains the secret capability cookie
		return true;
	}
	if( stricmp(name,ATTR_CLAIM_IDS) == 0 ) {
			// This attribute contains secret capability cookies
		return true;
	}
	if( stricmp(name,ATTR_TRANSFER_KEY) == 0 ) {
			// This attribute contains the secret file transfer cookie
		return true;
	}
	return false;
}

bool ClassAd::
Insert( const std::string &attrName, classad::ExprTree *expr )
{
	return classad::ClassAd::Insert( attrName, expr );
}

int ClassAd::
Insert( const char *name, classad::ExprTree *expr )
{
	string str = name;
	return Insert( str, expr ) ? TRUE : FALSE;
}

int ClassAd::
Insert( const char *str )
{
	classad::ClassAdParser parser;
	classad::ClassAd *newAd;

		// String escaping is different between new and old ClassAds.
		// We need to convert the escaping from old to new style before
		// handing the expression to the new ClassAds parser.
	string newAdStr = "[";
	for ( int i = 0; str[i] != '\0'; i++ ) {
		if ( str[i] == '\\' && 
			 ( str[i + 1] != '"' ||
			   str[i + 1] == '"' &&
			   ( str[i + 2] == '\0' || str[i + 2] == '\n' ||
				 str[i + 2] == '\r') ) ) {
			newAdStr.append( 1, '\\' );
		}
		newAdStr.append( 1, str[i] );
	}
	newAdStr += "]";
	newAd = parser.ParseClassAd( newAdStr );
	if ( newAd == NULL ) {
		return FALSE;
	}
	if ( newAd->size() != 1 ) {
		delete newAd;
		return FALSE;
	}
	
	iterator itr = newAd->begin();
	if ( !classad::ClassAd::Insert( itr->first, itr->second->Copy() ) ) {
		delete newAd;
		return FALSE;
	}
	delete newAd;
	return TRUE;
}

int ClassAd::
AssignExpr(char const *name,char const *value)
{
	classad::ClassAdParser par;
	classad::ExprTree *expr = NULL;

	if ( value == NULL ) {
		value = "Undefined";
	}
	if ( !par.ParseExpression( ConvertEscapingOldToNew( value ), expr, true ) ) {
		return FALSE;
	}
	if ( !Insert( name, expr ) ) {
		delete expr;
		return FALSE;
	}
	return TRUE;
}

int ClassAd::
Assign(char const *name,char const *value)
{
	if ( value == NULL ) {
		return AssignExpr( name, NULL );
	} else {
		return InsertAttr( name, value ) ? TRUE : FALSE;
	}
}

//  void ClassAd::
//  ResetExpr() { this->ptrExpr = exprList; }

//  ExprTree* ClassAd::
//  NextExpr(){}

//  void ClassAd::
//  ResetName() { this->ptrName = exprList; }

//  const char* ClassAd::
//  NextNameOriginal(){}


//  ExprTree* ClassAd::
//  Lookup(char *) const{}

//  ExprTree* ClassAd::
//  Lookup(const char*) const{}

int ClassAd::
LookupString( const char *name, char *value ) const 
{
	string strVal;
	if( !EvaluateAttrString( string( name ), strVal ) ) {
		return 0;
	}
	strcpy( value, strVal.c_str( ) );
	return 1;
} 

int ClassAd::
LookupString(const char *name, char *value, int max_len) const
{
	string strVal;
	if( !EvaluateAttrString( string( name ), strVal ) ) {
		return 0;
	}
	strncpy( value, strVal.c_str( ), max_len );
	return 1;
}

int ClassAd::
LookupString (const char *name, char **value) const 
{
	string strVal;
	if( !EvaluateAttrString( string( name ), strVal ) ) {
		return 0;
	}
	const char *strValCStr = strVal.c_str( );
	*value = (char *) malloc(strlen(strValCStr) + 1);
	if (*value != NULL) {
		strcpy(*value, strValCStr);
		return 1;
	}

	return 0;
}

int ClassAd::
LookupString( const char *name, MyString &value ) const 
{
	string strVal;
	if( !EvaluateAttrString( string( name ), strVal ) ) {
		return 0;
	}
	value = strVal.c_str();
	return 1;
} 

int ClassAd::
LookupInteger( const char *name, int &value ) const 
{
	bool    boolVal;
	int     haveInteger;
	string  sName;
	int		tmp_val;

	sName = string(name);
	if( EvaluateAttrInt(sName, tmp_val ) ) {
		value = tmp_val;
		haveInteger = TRUE;
	} else if( EvaluateAttrBool(sName, boolVal ) ) {
		value = boolVal ? 1 : 0;
		haveInteger = TRUE;
	} else {
		haveInteger = FALSE;
	}
	return haveInteger;
}

int ClassAd::
LookupFloat( const char *name, float &value ) const
{
	double  doubleVal;
	int     intVal;
	int     haveFloat;

	if(EvaluateAttrReal( string( name ), doubleVal ) ) {
		haveFloat = TRUE;
		value = (float) doubleVal;
	} else if(EvaluateAttrInt( string( name ), intVal ) ) {
		haveFloat = TRUE;
		value = (float)intVal;
	} else {
		haveFloat = FALSE;
	}
	return haveFloat;
}

int ClassAd::
LookupBool( const char *name, int &value ) const
{
	int   intVal;
	bool  boolVal;
	int haveBool;
	string sName;

	sName = string(name);

	if (EvaluateAttrBool(name, boolVal)) {
		haveBool = true;
		value = boolVal ? 1 : 0;
	} else if (EvaluateAttrInt(name, intVal)) {
		haveBool = true;
		value = (intVal != 0) ? 1 : 0;
	} else {
		haveBool = false;
	}
	return haveBool;
}

int ClassAd::
LookupBool( const char *name, bool &value ) const
{
	int   intVal;
	bool  boolVal;
	int haveBool;
	string sName;

	sName = string(name);

	if (EvaluateAttrBool(name, boolVal)) {
		haveBool = true;
		value = boolVal ? true : false;
	} else if (EvaluateAttrInt(name, intVal)) {
		haveBool = true;
		value = (intVal != 0) ? true : false;
	} else {
		haveBool = false;
	}
	return haveBool;
}

int ClassAd::
EvalString( const char *name, classad::ClassAd *target, char *value )
{
	int rc = 0;
	string strVal;

	if( target == this || target == NULL ) {
		if( EvaluateAttrString( name, strVal ) ) {
			strcpy( value, strVal.c_str( ) );
			return 1;
		}
		return 0;
	}

	classad::MatchClassAd mad( this, target );
	if( this->Lookup( name ) ) {
		if( this->EvaluateAttrString( name, strVal ) ) {
			strcpy( value, strVal.c_str( ) );
			rc = 1;
		}
	} else if( target->Lookup( name ) ) {
		if( target->EvaluateAttrString( name, strVal ) ) {
			strcpy( value, strVal.c_str( ) );
			rc = 1;
		}
	}
	mad.RemoveLeftAd( );
	mad.RemoveRightAd( );
	return rc;
}

/*
 * Ensure that we allocate the value, so we have sufficient space
 */
int ClassAd::
EvalString (const char *name, classad::ClassAd *target, char **value)
{
    
	string strVal;
    bool foundAttr = false;

	if( target == this || target == NULL ) {
		if( EvaluateAttrString( name, strVal ) ) {

            *value = (char *)malloc(strlen(strVal.c_str()) + 1);
            if(*value != NULL) {
                strcpy( *value, strVal.c_str( ) );
                return 1;
            } else {
                return 0;
            }
		}
		return 0;
	}

	classad::MatchClassAd mad( this, target );

    if( this->Lookup(name) ) {

        if( this->EvaluateAttrString( name, strVal ) ) {
            foundAttr = true;
        }		
    } else if( target->Lookup(name) ) {
        if( this->EvaluateAttrString( name, strVal ) ) {
            foundAttr = true;
        }		
    }

    if(foundAttr)
    {
        *value = (char *)malloc(strlen(strVal.c_str()) + 1);
        if(*value != NULL) {
            strcpy( *value, strVal.c_str( ) );
            mad.RemoveLeftAd( );
            mad.RemoveRightAd( );
            return 1;
        }
    }

	mad.RemoveLeftAd( );
	mad.RemoveRightAd( );
	return 0;
}

int ClassAd::
EvalString(const char *name, classad::ClassAd *target, MyString & value)
{
    char * pvalue = NULL;
    //this one makes sure length is good
    int ret = EvalString(name, target, &pvalue); 
    if(ret == 0) { return ret; }
    value = pvalue;
    free(pvalue);
    return ret;
}

int ClassAd::
EvalInteger (const char *name, classad::ClassAd *target, int &value)
{
	int rc = 0;
	int tmp_val;

	if( target == this || target == NULL ) {
		if( EvaluateAttrInt( name, tmp_val ) ) { 
			value = tmp_val;
			return 1;
		}
		return 0;
	}

	classad::MatchClassAd mad( this, target );
	if( this->Lookup( name ) ) {
		if( this->EvaluateAttrInt( name, tmp_val ) ) {
			value = tmp_val;
			rc = 1;
		}
	} else if( target->Lookup( name ) ) {
		if( target->EvaluateAttrInt( name, tmp_val ) ) {
			value = tmp_val;
			rc = 1;
		}
	}
	mad.RemoveLeftAd( );
	mad.RemoveRightAd( );
	return rc;
}

int ClassAd::
EvalFloat (const char *name, classad::ClassAd *target, float &value)
{
	int rc = 0;
	classad::Value val;
	double doubleVal;
	int intVal;

	if( target == this || target == NULL ) {
		if( EvaluateAttr( name, val ) ) {
			if( val.IsRealValue( doubleVal ) ) {
				value = ( float )doubleVal;
				return 1;
			}
			if( val.IsIntegerValue( intVal ) ) {
				value = ( float )intVal;
				return 1;
			}
		}
		return 0;
	}

	classad::MatchClassAd mad( this, target );
	if( this->Lookup( name ) ) {
		if( this->EvaluateAttr( name, val ) ) {
			if( val.IsRealValue( doubleVal ) ) {
				value = ( float )doubleVal;
				rc = 1;
			}
			if( val.IsIntegerValue( intVal ) ) {
				value = ( float )intVal;
				rc = 1;
			}
		}
	} else if( target->Lookup( name ) ) {
		if( target->EvaluateAttr( name, val ) ) {
			if( val.IsRealValue( doubleVal ) ) {
				value = ( float )doubleVal;
				rc = 1;
			}
			if( val.IsIntegerValue( intVal ) ) {
				value = ( float )intVal;
				rc = 1;
			}
		}
	}
	mad.RemoveLeftAd( );
	mad.RemoveRightAd( );
	return rc;
}

int ClassAd::
EvalBool  (const char *name, classad::ClassAd *target, int &value)
{
	int rc = 0;
	classad::Value val;
	double doubleVal;
	int intVal;
	bool boolVal;

	if( target == this || target == NULL ) {
		if( EvaluateAttr( name, val ) ) {
			if( val.IsBooleanValue( boolVal ) ) {
				value = boolVal ? 1 : 0;
				return 1;
			}
			if( val.IsIntegerValue( value ) ) {
				value = intVal ? 1 : 0;
				return 1;
			}
			if( val.IsRealValue( doubleVal ) ) {
				value = doubleVal ? 1 : 0;
				return 1;
			}
		}
		return 0;
	}

	classad::MatchClassAd mad( this, target );
	if( this->Lookup( name ) ) {
		if( this->EvaluateAttr( name, val ) ) {
			if( val.IsBooleanValue( boolVal ) ) {
				value = boolVal ? 1 : 0;
				rc = 1;
			}
			if( val.IsIntegerValue( intVal ) ) {
				value = intVal ? 1 : 0;
				rc = 1;
			}
			if( val.IsRealValue( doubleVal ) ) {
				value = doubleVal ? 1 : 0;
				rc = 1;
			}
		}
	} else if( target->Lookup( name ) ) {
		if( target->EvaluateAttr( name, val ) ) {
			if( val.IsBooleanValue( boolVal ) ) {
				value = boolVal ? 1 : 0;
				rc = 1;
			}
			if( val.IsIntegerValue( intVal ) ) {
				value = intVal ? 1 : 0;
				rc = 1;
			}
			if( val.IsRealValue( doubleVal ) ) {
				value = doubleVal ? 1 : 0;
				rc = 1;
			}
		}
	}

	mad.RemoveLeftAd( );
	mad.RemoveRightAd( );
	return rc;
}

bool ClassAd::
initFromString( char const *str,MyString *err_msg )
{
	bool succeeded = true;

	// First, clear our ad so we start with a fresh ClassAd
	Clear();

		// Reinsert CurrentTime, emulating the special version in old
		// ClassAds
	AssignExpr( ATTR_CURRENT_TIME, "time()" );

	char *exprbuf = new char[strlen(str)+1];
	ASSERT( exprbuf );

	while( *str ) {
		while( isspace(*str) ) {
			str++;
		}

		size_t len = strcspn(str,"\n");
		strncpy(exprbuf,str,len);
		exprbuf[len] = '\0';

		if( str[len] == '\n' ) {
			len++;
		}
		str += len;

		if (!Insert(exprbuf)) {
			if( err_msg ) {
				err_msg->sprintf("Failed to parse ClassAd expression: %s",
					exprbuf);
			} else {
				dprintf(D_ALWAYS,"Failed to parse ClassAd expression: %s\n",
					exprbuf);
			}
			succeeded = false;
			break;
		}
	}

	delete [] exprbuf;
	return succeeded;
}

        // shipping functions
int ClassAd::
put( Stream &s )
{
	if( !putOldClassAd( &s, *this, m_privateAttrsAreInvisible ) ) {
		return FALSE;
	}
	return TRUE;
}

int ClassAd::
initFromStream(Stream& s)
{
	if( !getOldClassAd( &s, *this ) ) {
		return FALSE;
	}

		// Reinsert CurrentTime, emulating the special version in old
		// ClassAds
	AssignExpr( ATTR_CURRENT_TIME, "time()" );

	return TRUE;
}

int ClassAd::
putAttrList( Stream &s )
{
	if( !putOldClassAdNoTypes( &s, *this, m_privateAttrsAreInvisible ) ) {
		return FALSE;
	}
	return TRUE;
}

int ClassAd::
initAttrListFromStream(Stream& s)
{
	if( !getOldClassAdNoTypes( &s, *this ) ) {
		return FALSE;
	}

		// Reinsert CurrentTime, emulating the special version in old
		// ClassAds
	AssignExpr( ATTR_CURRENT_TIME, "time()" );

	return TRUE;
}

		// output functions
int	ClassAd::
fPrint( FILE *file, StringList *attr_white_list )
{
	MyString buffer;

	sPrint( buffer, attr_white_list );
	fprintf( file, "%s", buffer.Value() );

	return TRUE;
}

void ClassAd::
dPrint( int level )
{
	MyString buffer;

	SetPrivateAttributesInvisible( true );
	sPrint( buffer );
	SetPrivateAttributesInvisible( false );

	dprintf( level|D_NOHEADER, "%s", buffer.Value() );
}

int ClassAd::
sPrint( MyString &output, StringList *attr_white_list )
{
	classad::ClassAd::iterator itr;

	classad::ClassAdUnParser unp;
	unp.SetOldClassAd( true );
	string value;

	output = "";

	classad::ClassAd *parent = GetChainedParentAd();

	if ( parent ) {
		for ( itr = parent->begin(); itr != parent->end(); itr++ ) {
			if ( attr_white_list && !attr_white_list->contains_anycase(itr->first.c_str()) ) {
				continue; // not in white-list
			}
			if ( !m_privateAttrsAreInvisible ||
				 !ClassAdAttributeIsPrivate( itr->first.c_str() ) ) {
				value = "";
				unp.Unparse( value, itr->second );
				output.sprintf_cat( "%s = %s\n", itr->first.c_str(),
									value.c_str() );
			}
		}
	}

	for ( itr = this->begin(); itr != this->end(); itr++ ) {
		if ( attr_white_list && !attr_white_list->contains_anycase(itr->first.c_str()) ) {
			continue; // not in white-list
		}
		if ( !m_privateAttrsAreInvisible ||
			 !ClassAdAttributeIsPrivate( itr->first.c_str() ) ) {
			value = "";
			unp.Unparse( value, itr->second );
			output.sprintf_cat( "%s = %s\n", itr->first.c_str(),
								value.c_str() );
		}
	}

	return TRUE;
}
// Taken from the old classad's function. Got rid of incorrect documentation. 
////////////////////////////////////////////////////////////////////////////////// Print an expression with a certain name into a buffer. 
// The caller should pass the size of the buffer in buffersize.
// If buffer is NULL, then space will be allocated with malloc(), and it needs
// to be free-ed with free() by the user.
////////////////////////////////////////////////////////////////////////////////
char* ClassAd::
sPrintExpr(char* buffer, unsigned int buffersize, const char* name)
{

	classad::ClassAdUnParser unp;
    string parsedString;
	classad::ExprTree* expr;

	unp.SetOldClassAd( true );

    expr = Lookup(name); 

    if(!expr)
    {
        return NULL;
    }

    unp.Unparse(parsedString, expr);

    if(!buffer)
    {

        buffersize = strlen(name) + parsedString.length() +
                        3 +     // " = "
                        1;      // null termination
        buffer = (char*) malloc(buffersize);
        
    } 

    snprintf(buffer, buffersize, "%s = %s", name, parsedString.c_str() );
    buffer[buffersize - 1] = '\0';

    return buffer;
}

// ClassAd methods

		// Type operations
void ClassAd::
SetMyTypeName( const char *myType )
{
	if( myType ) {
		InsertAttr( ATTR_MY_TYPE, string( myType ) );
	}

	return;
}

const char*	ClassAd::
GetMyTypeName( ) const
{
	string myTypeStr;
	if( !EvaluateAttrString( ATTR_MY_TYPE, myTypeStr ) ) {
		return "";
	}
	return myTypeStr.c_str( );
}

void ClassAd::
SetTargetTypeName( const char *targetType )
{
	if( targetType ) {
		InsertAttr( ATTR_TARGET_TYPE, string( targetType ) );
	}

	return;
}

const char*	ClassAd::
GetTargetTypeName( ) const
{
	string targetTypeStr;
	if( !EvaluateAttrString( ATTR_TARGET_TYPE, targetTypeStr ) ) {
		return "";
	}
	return targetTypeStr.c_str( );
}

void ClassAd::
ResetExpr()
{
	m_exprItr = begin();
	m_exprItrInChain = false;
    //this'll originally be null
    m_dirtyItrInit = false;
}

void ClassAd::
ResetName()
{
	m_nameItr = begin();
	m_nameItrInChain = false;
}

const char *ClassAd::
NextNameOriginal()
{
	const char *name = NULL;
	classad::ClassAd *chained_ad = GetChainedParentAd();
	// After iterating through all the names in this ad,
	// get all the names in our chained ad as well.
	if ( m_nameItr == end() && chained_ad && !m_nameItrInChain ) {
		m_nameItr = chained_ad->begin();
		m_nameItrInChain = true;
	}
	if ( m_nameItr == end() ||
		 m_nameItrInChain && m_nameItr == chained_ad->end() ) {
		return NULL;
	}
	name = m_nameItr->first.c_str();
	m_nameItr++;
	return name;
}

// Back compatibility helper methods

bool ClassAd::
AddExplicitConditionals( classad::ExprTree *expr, classad::ExprTree *&newExpr )
{
	if( expr == NULL ) {
		return false;
	}
	newExpr = AddExplicitConditionals( expr );
	return true;
}

void ClassAd::AddExplicitTargetRefs(  ) 
{
	set< string, classad::CaseIgnLTStr > definedAttrs;
	for( classad::AttrList::iterator a = begin( ); a != end( ); a++ ) {
		definedAttrs.insert(a->first);
	}
	
	for( classad::AttrList::iterator a = begin( ); a != end( ); a++ ) {
		if ( a->second->GetKind() != classad::ExprTree::LITERAL_NODE ) {
			this->Insert( a->first,
						  compat_classad::AddExplicitTargetRefs( a->second, definedAttrs )) ;
		}
	}
}

void ClassAd::RemoveExplicitTargetRefs( )
{
	for( classad::AttrList::iterator a = begin( ); a != end( ); a++ ) {
		if ( a->second->GetKind() != classad::ExprTree::LITERAL_NODE ) {
			this->Insert( a->first, 
						  compat_classad::RemoveExplicitTargetRefs( a->second ) );
		}
	}
}  


void ClassAd:: 
AddTargetRefs( TargetAdType target_type )
{
	for( classad::AttrList::iterator a = begin(); a != end(); a++ ) {
		if ( a->second->GetKind() != classad::ExprTree::LITERAL_NODE ) {
			this->Insert( a->first, 
						  compat_classad::AddTargetRefs( a->second, target_type ) );
		}
	}
}

classad::ExprTree *ClassAd::
AddExplicitConditionals( classad::ExprTree *expr )
{
	if( expr == NULL ) {
		return NULL;
	}
	classad::ExprTree::NodeKind nKind = expr->GetKind( );
	switch( nKind ) {
	case classad::ExprTree::ATTRREF_NODE: {
			// replace "attr" with "(IsBoolean(attr) ? ( attr ? 1 : 0) : attr)"
		classad::ExprTree *fnExpr = NULL;
		vector< classad::ExprTree * > params( 1 );
		params[0] = expr->Copy( );
		classad::ExprTree *condExpr = NULL;
		classad::ExprTree *parenExpr = NULL;
		classad::ExprTree *condExpr2 = NULL;
		classad::ExprTree *parenExpr2 = NULL;
		classad::Value val0, val1;
		val0.SetIntegerValue( 0 );
		val1.SetIntegerValue( 1 );
		fnExpr = classad::FunctionCall::MakeFunctionCall( "IsBoolean", params );
		condExpr = classad::Operation::MakeOperation( classad::Operation::TERNARY_OP,
										expr->Copy( ), 
										classad::Literal::MakeLiteral( val1 ),
										classad::Literal::MakeLiteral( val0 ) );
		parenExpr = classad::Operation::MakeOperation( classad::Operation::PARENTHESES_OP,
											  condExpr, NULL, NULL );
		condExpr2 = classad::Operation::MakeOperation( classad::Operation::TERNARY_OP,
											  fnExpr, parenExpr, 
											  expr->Copy( ) );
		parenExpr2 = classad::Operation::MakeOperation( classad::Operation::PARENTHESES_OP,
										 condExpr2, NULL, NULL );
		return parenExpr2;
	}
	case classad::ExprTree::FN_CALL_NODE:
	case classad::ExprTree::CLASSAD_NODE:
	case classad::ExprTree::EXPR_LIST_NODE: {
		return NULL;
	}
	case classad::ExprTree::LITERAL_NODE: {
		classad::Value val;
		( ( classad::Literal *)expr )->GetValue( val );
		bool b;
		if( val.IsBooleanValue( b ) ) {
			if( b ) {
					// replace "true" with "1"
				val.SetIntegerValue( 1 );
			}
			else {
					// replace "false" with "0"
				val.SetIntegerValue( 0 );
			}
			return classad::Literal::MakeLiteral( val );
		}
		else {
			return NULL;
		}
	}
	case classad::ExprTree::OP_NODE: {
		classad::Operation::OpKind oKind;
		classad::ExprTree * expr1 = NULL;
		classad::ExprTree * expr2 = NULL;
		classad::ExprTree * expr3 = NULL;
		( ( classad::Operation * )expr )->GetComponents( oKind, expr1, expr2, expr3 );
		if ( oKind == classad::Operation::PARENTHESES_OP ) {
			ExprTree *newExpr1 = AddExplicitConditionals( expr1 );
			return classad::Operation::MakeOperation( classad::Operation::PARENTHESES_OP,
											 newExpr1, NULL, NULL );
		}
		else if( ( classad::Operation::__COMPARISON_START__ <= oKind &&
				   oKind <= classad::Operation::__COMPARISON_END__ ) ||
				 ( classad::Operation::__LOGIC_START__ <= oKind &&
				   oKind <= classad::Operation::__LOGIC_END__ ) ) {
				// Comparison/Logic Operation expression
				// replace "expr" with "expr ? 1 : 0"

			classad::ExprTree *newExpr = expr;
			if( oKind == classad::Operation::LESS_THAN_OP ||
				oKind == classad::Operation::LESS_OR_EQUAL_OP ||
				oKind == classad::Operation::GREATER_OR_EQUAL_OP ||
				oKind == classad::Operation::GREATER_THAN_OP ) {				
				classad::ExprTree *newExpr1 = AddExplicitConditionals( expr1 );
				classad::ExprTree *newExpr2 = AddExplicitConditionals( expr2 );
				if( newExpr1 != NULL || newExpr2 != NULL ) {
					if( newExpr1 == NULL ) {
						newExpr1 = expr1->Copy( );
					}
					if( newExpr2 == NULL ) {
						newExpr2 = expr2->Copy( );
					}
					newExpr = classad::Operation::MakeOperation( oKind, newExpr1,
														newExpr2, NULL );
				}
			}

			classad::Value val0, val1;
			val0.SetIntegerValue( 0 );
			val1.SetIntegerValue( 1 );
			classad::ExprTree *tern = NULL;
			tern = classad::Operation::MakeOperation( classad::Operation::TERNARY_OP,
											 newExpr->Copy( ),
											 classad::Literal::MakeLiteral( val1 ),
											 classad::Literal::MakeLiteral( val0 ) );
			return classad::Operation::MakeOperation( classad::Operation::PARENTHESES_OP,
											 tern, NULL, NULL );
		}
		else if( classad::Operation::__ARITHMETIC_START__ <= oKind &&
				 oKind <= classad::Operation::__ARITHMETIC_END__ ) {
			classad::ExprTree *newExpr1 = AddExplicitConditionals( expr1 );
			if( oKind == classad::Operation::UNARY_PLUS_OP || 
				oKind == classad::Operation::UNARY_MINUS_OP ) {
				if( newExpr1 != NULL ) {
					return classad::Operation::MakeOperation(oKind,newExpr1,NULL,NULL);
				}
				else {
					return NULL;
				}
			}
			else {
				classad::ExprTree *newExpr2 = AddExplicitConditionals( expr2 );
				if( newExpr1 != NULL || newExpr2 != NULL ) {
					if( newExpr1 == NULL ) {
						newExpr1 = expr1->Copy( );
					}
					if( newExpr2 == NULL ) {
						newExpr2 = expr2->Copy( );
					}
					return classad::Operation::MakeOperation( oKind, newExpr1, newExpr2,
													 NULL );
				}
				else {
					return NULL;
				}
			}
		}
		else if( oKind == classad::Operation::TERNARY_OP ) {
			ExprTree *newExpr2 = AddExplicitConditionals( expr2 );
			ExprTree *newExpr3 = AddExplicitConditionals( expr3 );
			if( newExpr2 != NULL || newExpr3 != NULL ) {
				if( newExpr2 == NULL ) {
					newExpr2 = expr2->Copy( );
				}
				if( newExpr3 == NULL ) {
					newExpr3 = expr3->Copy( );
				}
				return classad::Operation::MakeOperation( oKind, expr1->Copy( ), 
												 newExpr2, newExpr3 );
			}
			else {
				return NULL;
			}
		}
		return NULL;
	}
	default: {
		return NULL;
	}
	}
		
	return NULL;
}


// Determine if a value is valid to be written to the log. The value
// is a RHS of an expression. According to LogSetAttribute::WriteBody,
// the only invalid character is a '\n'.
bool ClassAd::
IsValidAttrValue(const char *value)
{
    //NULL value is not invalid, may translate to UNDEFINED
    if(!value)
    {
        return true;
    }

    //According to the old classad's docs, LogSetAttribute::WriteBody
    // says that the only invalid character for a value is '\n'.
    // But then it also includes '\r', so whatever.
    while (*value) {
        if(((*value) == '\n') ||
           ((*value) == '\r')) {
            return false;
        }
        value++;
    }

    return true;
}

//	Decides if a string is a valid attribute name, the LHS
//  of an expression.  As per the manual, valid names:
//
//  Attribute names are sequences of alphabetic characters, digits and 
//  underscores, and may not begin with a digit

/* static */ bool
ClassAd::IsValidAttrName(const char *name) {
		// NULL pointer certainly false
	if (!name) {
		return false;
	}

		// Must start with alpha or _
	if (!isalpha(*name) && *name != '_') {
		return false;
	}

	name++;

		// subsequent letters must be alphanum or _
	while (*name) {
		if (!isalnum(*name) && *name != '_') {
			return false;
		}
		name++;
	}

	return true;
}

bool ClassAd::NextExpr( const char *&name, ExprTree *&value )
{
	classad::ClassAd *chained_ad = GetChainedParentAd();
	// After iterating through all the attributes in this ad,
	// get all the attributes in our chained ad as well.
	if ( m_exprItr == end() && chained_ad && !m_exprItrInChain ) {
		m_exprItr = chained_ad->begin();
		m_exprItrInChain = true;
	}
	if ( m_exprItr == end() ||
		 m_exprItrInChain && m_exprItr == chained_ad->end() ) {
		return false;
	}
	name = m_exprItr->first.c_str();
	value = m_exprItr->second;
	m_exprItr++;
	return true;
}

//provides a way to get the next dirty expression in the set of 
//  dirty attributes.
bool ClassAd::
NextDirtyExpr(const char *&name, classad::ExprTree *&expr)
{
    //this'll reset whenever ResetDirtyItr is called
    if(!m_dirtyItrInit)
    {
        m_dirtyItr = dirtyBegin();
        m_dirtyItrInit = true;
    }

	name = NULL;
    expr = NULL;
    
    //get the next dirty attribute if we aren't past the end.
    if( m_dirtyItr == dirtyEnd() )
    {
        return false;
    }
    
    //figure out what exprtree it is related to
	name = m_dirtyItr->c_str();
    expr = classad::ClassAd::Lookup(*m_dirtyItr);
    m_dirtyItr++;

    return true;

}

void ClassAd::
SetDirtyFlag(const char *name, bool dirty)
{
	if ( dirty ) {
		MarkAttributeDirty( name );
	} else {
		MarkAttributeClean( name );
	}
}

void ClassAd::
GetDirtyFlag(const char *name, bool *exists, bool *dirty)
{
	if ( Lookup( name ) == NULL ) {
		if ( exists ) {
			*exists = false;
		}
		return;
	}
	if ( exists ) {
		*exists = true;
	}
	if ( dirty ) {
		*dirty = IsAttributeDirty( name );
	}
}

void ClassAd::
CopyAttribute( char const *target_attr, classad::ClassAd *source_ad )
{
	CopyAttribute( target_attr, target_attr, source_ad );
}


void ClassAd::
CopyAttribute( char const *target_attr, char const *source_attr,
			   classad::ClassAd *source_ad )
{
	ASSERT( target_attr );
	ASSERT( source_attr );
	if( !source_ad ) {
		source_ad = this;
	}

	classad::ExprTree *e = source_ad->Lookup( source_attr );
	if ( e ) {
		Insert( target_attr, e->Copy() );
	} else {
		Delete( target_attr );
	}
}

//////////////XML functions///////////

int ClassAd::
fPrintAsXML(FILE *fp)
{
    if(!fp)
    {
        return FALSE;
    }

    MyString out;
    sPrintAsXML(out);
    fprintf(fp, "%s", out.Value());
    return TRUE;
}

int ClassAd::
sPrintAsXML(MyString &output, StringList *attr_white_list)
{
	ClassAdXMLUnparser  unparser;
	MyString            xml;
	unparser.SetUseCompactSpacing(false);
	unparser.Unparse(this, xml, attr_white_list);
	output += xml;
	return TRUE;
}
///////////// end XML functions /////////

char const *
ClassAd::EscapeStringValue(char const *val, MyString &buf)
{
    if(val == NULL)
        return NULL;

    classad::Value tmpValue;
    string stringToAppeaseUnparse;
    classad::ClassAdUnParser unparse;

	unparse.SetOldClassAd( true );

    tmpValue.SetStringValue(val);
    unparse.Unparse(stringToAppeaseUnparse, tmpValue);

    buf = stringToAppeaseUnparse.c_str();
	buf = buf.Substr( 1, buf.Length() - 2 );
    return buf.Value();
}

void ClassAd::ChainCollapse()
{
    classad::ExprTree *tmpExprTree;

	classad::ClassAd *parent = GetChainedParentAd();

    if(!parent)
    {   
        //nothing chained, time to leave
        return;
    }

    Unchain();

    classad::AttrList::iterator itr; 

    for(itr = parent->begin(); itr != parent->end(); itr++)
    {
        // Only move the value from our chained ad into our ad when it 
        // does not already exist. Hence the Lookup(). 
        // This means that the attributes in our classad takes precedence
        // over the ones in the chained class ad.

        if( !Lookup((*itr).first) )
        {
            tmpExprTree = (*itr).second;     

            //deep copy it!
            tmpExprTree = tmpExprTree->Copy(); 
            ASSERT(tmpExprTree); 

            //K, it's clear. Insert it!
            Insert((*itr).first, tmpExprTree);
        }
    }
}

void ClassAd::
GetReferences(const char* attr,
                StringList &internal_refs,
                StringList &external_refs)
{
    ExprTree *tree;

    tree = Lookup(attr);
    if(tree != NULL)
    {
		_GetReferences( tree, internal_refs, external_refs );
    }
}

bool ClassAd::
GetExprReferences(const char* expr,
				  StringList &internal_refs,
				  StringList &external_refs)
{
	classad::ClassAdParser par;
	classad::ExprTree *tree = NULL;

    if ( !par.ParseExpression( ConvertEscapingOldToNew( expr ), tree, true ) ) {
        return false;
    }

	_GetReferences( tree, internal_refs, external_refs );

	delete tree;

	return true;
}

void ClassAd::
_GetReferences(classad::ExprTree *tree,
			   StringList &internal_refs,
			   StringList &external_refs)
{
	if ( tree == NULL ) {
		return;
	}

	classad::References ext_refs_set;
	classad::References int_refs_set;
	classad::References::iterator set_itr;
	GetExternalReferences(tree, ext_refs_set, true);
	GetInternalReferences(tree, int_refs_set, true);

	for ( set_itr = ext_refs_set.begin(); set_itr != ext_refs_set.end();
		  set_itr++ ) {
		const char *name = set_itr->c_str();
		if ( strncasecmp( name, "target.", 7 ) ) {
			external_refs.append( set_itr->c_str() );
		} else {
			external_refs.append( &set_itr->c_str()[7] );
		}
	}

	for ( set_itr = int_refs_set.begin(); set_itr != int_refs_set.end();
		  set_itr++ ) {
		internal_refs.append( set_itr->c_str() );
	}
}



// the freestanding functions 

classad::ExprTree *
AddExplicitTargetRefs(classad::ExprTree *tree,
						std::set < std::string, classad::CaseIgnLTStr > & definedAttrs) 
{
	if( tree == NULL ) {
		return NULL;
	}
	classad::ExprTree::NodeKind nKind = tree->GetKind( );
	switch( nKind ) {
	case classad::ExprTree::ATTRREF_NODE: {
		classad::ExprTree *expr = NULL;
		string attr = "";
		bool abs = false;
		( ( classad::AttributeReference * )tree )->GetComponents(expr,attr,abs);
		if( abs || expr != NULL ) {
			return tree->Copy( );
		}
		else {
			if( definedAttrs.find( attr ) == definedAttrs.end( ) ) {
					// attribute is not defined, so insert "target"
				classad::AttributeReference *target = NULL;
				target = classad::AttributeReference::MakeAttributeReference(NULL,
																	"target");
				return classad::AttributeReference::MakeAttributeReference(target,attr);
			}
			else {
				return tree->Copy( );
			}
		}
	}
	case classad::ExprTree::OP_NODE: {
		classad::Operation::OpKind oKind;
		classad::ExprTree * expr1 = NULL;
		classad::ExprTree * expr2 = NULL;
		classad::ExprTree * expr3 = NULL;
		classad::ExprTree * newExpr1 = NULL;
		classad::ExprTree * newExpr2 = NULL;
		classad::ExprTree * newExpr3 = NULL;
		( ( classad::Operation * )tree )->GetComponents( oKind, expr1, expr2, expr3 );
		if( expr1 != NULL ) {
			newExpr1 = AddExplicitTargetRefs( expr1, definedAttrs );
		}
		if( expr2 != NULL ) {
			newExpr2 = AddExplicitTargetRefs( expr2, definedAttrs );
		}
		if( expr3 != NULL ) {
			newExpr3 = AddExplicitTargetRefs( expr3, definedAttrs );
		}
		return classad::Operation::MakeOperation( oKind, newExpr1, newExpr2, newExpr3 );
	}
	case classad::ExprTree::FN_CALL_NODE: {
		std::string fn_name;
		classad::ArgumentList old_fn_args;
		classad::ArgumentList new_fn_args;
		( ( classad::FunctionCall * )tree )->GetComponents( fn_name, old_fn_args );
		for ( classad::ArgumentList::iterator i = old_fn_args.begin(); i != old_fn_args.end(); i++ ) {
			new_fn_args.push_back( AddExplicitTargetRefs( *i, definedAttrs ) );
		}
		return classad::FunctionCall::MakeFunctionCall( fn_name, new_fn_args );
	}
	default: {
 			// old ClassAds have no function calls, nested ClassAds or lists
			// literals have no attrrefs in them
		return tree->Copy( );
	}
	}
} 

classad::ExprTree *
AddExplicitTargetRefs(classad::ExprTree *eTree, classad::ClassAd *ad ) 
{
	set< string, classad::CaseIgnLTStr > definedAttrs;
	
	for( classad::AttrList::iterator a = ad->begin( ); a != ad->end( ); a++ ) {
		definedAttrs.insert( a->first );
	}
	return AddExplicitTargetRefs(eTree, definedAttrs);
}

classad::ExprTree *RemoveExplicitTargetRefs( classad::ExprTree *tree )
{
	if( tree == NULL ) {
		return NULL;
	}
	classad::ExprTree::NodeKind nKind = tree->GetKind( );
	switch( nKind ) {
	case classad::ExprTree::ATTRREF_NODE: {
		classad::ExprTree *expr = NULL;
		string attr = "";
		bool abs = false;
		( ( classad::AttributeReference * )tree )->GetComponents(expr,attr,abs);
		if(!abs && (expr != NULL)) {
			string newAttr = "";
			classad::ExprTree *exp = NULL;
			abs = false;
			( ( classad::AttributeReference * )expr )->GetComponents(exp,newAttr,abs);
			if (strcmp(newAttr.c_str(), "target") == 0){
				classad::AttributeReference *noTarget = NULL;
				noTarget = classad::AttributeReference::MakeAttributeReference(exp,"",abs);
				return classad::AttributeReference::MakeAttributeReference(noTarget,attr);
			}	 
		} 
		return tree->Copy();
	}
	case classad::ExprTree::OP_NODE: {
		classad::Operation::OpKind oKind;
		classad::ExprTree * expr1 = NULL;
		classad::ExprTree * expr2 = NULL;
		classad::ExprTree * expr3 = NULL;
		classad::ExprTree * newExpr1 = NULL;
		classad::ExprTree * newExpr2 = NULL;
		classad::ExprTree * newExpr3 = NULL;
		( ( classad::Operation * )tree )->GetComponents( oKind, expr1, expr2, expr3 );
		if( expr1 != NULL ) {
			newExpr1 = RemoveExplicitTargetRefs( expr1  );
		}
		if( expr2 != NULL ) {
			newExpr2 = RemoveExplicitTargetRefs( expr2 );
		}
		if( expr3 != NULL ) {
			newExpr3 = RemoveExplicitTargetRefs( expr3 );
		}
		return classad::Operation::MakeOperation( oKind, newExpr1, newExpr2, newExpr3 );
	}
	case classad::ExprTree::FN_CALL_NODE: {
		std::string fn_name;
		classad::ArgumentList old_fn_args;
		classad::ArgumentList new_fn_args;
		( ( classad::FunctionCall * )tree )->GetComponents( fn_name, old_fn_args );
		for ( classad::ArgumentList::iterator i = old_fn_args.begin(); i != old_fn_args.end(); i++ ) {
			new_fn_args.push_back( RemoveExplicitTargetRefs( *i ) );
		}
		return classad::FunctionCall::MakeFunctionCall( fn_name, new_fn_args );
	}
	default: {
 			// old ClassAds have no function calls, nested ClassAds or lists
			// literals have no attrrefs in them
		return tree->Copy( );
	}
	}
}	

static ClassAd job_attrs_ad;
static ClassAd machine_attrs_ad;
static StringList job_attrs_strlist;
static StringList machine_attrs_strlist;

static bool target_attrs_init = false;

static const char *machine_attrs_list[] = {
	ATTR_NAME,
	ATTR_MACHINE,
	ATTR_AVAIL_TIME,
	ATTR_LAST_AVAIL_INTERVAL,
	ATTR_AVAIL_SINCE,
	ATTR_AVAIL_TIME_ESTIMATE,
	ATTR_IS_VALID_CHECKPOINT_PLATFORM,
	ATTR_WITHIN_RESOURCE_LIMITS,
	ATTR_RUNNING_COD_JOB,
	ATTR_ARCH,
	ATTR_OPSYS,
	ATTR_HAS_IO_PROXY,
	ATTR_CHECKPOINT_PLATFORM,
	ATTR_TOTAL_VIRTUAL_MEMORY,
	ATTR_TOTAL_CPUS,
	ATTR_TOTAL_MEMORY,
	ATTR_KFLOPS,
	ATTR_MIPS,
	ATTR_LOCAL_CREDD,
	ATTR_LAST_BENCHMARK,
	ATTR_TOTAL_LOAD_AVG,
	ATTR_TOTAL_CONDOR_LOAD_AVG,
	ATTR_CLOCK_MIN,
	ATTR_CLOCK_DAY,
	ATTR_RUN_BENCHMARKS,
	ATTR_VIRTUAL_MEMORY,
	ATTR_TOTAL_DISK,
	ATTR_DISK,
	ATTR_CONDOR_LOAD_AVG,
	ATTR_LOAD_AVG,
	ATTR_KEYBOARD_IDLE,
	ATTR_CONSOLE_IDLE,
	ATTR_MEMORY,
	ATTR_CPUS,
	ATTR_MAX_JOB_RETIREMENT_TIME,
	ATTR_FETCH_WORK_DELAY,
	ATTR_UNHIBERNATE,
	ATTR_MACHINE_LAST_MATCH_TIME,
	ATTR_SLOT_WEIGHT,
	ATTR_IS_OWNER,
	ATTR_CPU_BUSY,
	ATTR_TOTAL_SLOTS,
	ATTR_TOTAL_VIRTUAL_MACHINES,
	ATTR_STATE,
	ATTR_LAST_HEARD_FROM,
	ATTR_ENTERED_CURRENT_STATE,
	ATTR_ACTIVITY,
	ATTR_ENTERED_CURRENT_ACTIVITY,
	ATTR_TOTAL_TIME_OWNER_IDLE,
	ATTR_TOTAL_TIME_UNCLAIMED_IDLE,
	ATTR_TOTAL_TIME_UNCLAIMED_BENCHMARKING,
	ATTR_TOTAL_TIME_MATCHED_IDLE,
	ATTR_TOTAL_TIME_CLAIMED_IDLE,
	ATTR_TOTAL_TIME_CLAIMED_BUSY,
	ATTR_TOTAL_TIME_CLAIMED_SUSPENDED,
	ATTR_TOTAL_TIME_CLAIMED_RETIRING,
	ATTR_TOTAL_TIME_PREEMPTING_VACATING,
	ATTR_TOTAL_TIME_PREEMPTING_KILLING,
	ATTR_TOTAL_TIME_BACKFILL_IDLE,
	ATTR_TOTAL_TIME_BACKFILL_BUSY,
	ATTR_TOTAL_TIME_BACKFILL_KILLING,
	ATTR_CPU_IS_BUSY,
	ATTR_CPU_BUSY_TIME,
	ATTR_VIRTUAL_MACHINE_ID,
	ATTR_SLOT_PARTITIONABLE,
	ATTR_SLOT_DYNAMIC,
	ATTR_LAST_FETCH_WORK_SPAWNED,
	ATTR_LAST_FETCH_WORK_COMPLETED,
	ATTR_NEXT_FETCH_WORK_DELAY,
	ATTR_CURRENT_RANK,
	ATTR_REMOTE_USER,
	ATTR_REMOTE_OWNER,
	ATTR_CLIENT_MACHINE,
	ATTR_NUM_COD_CLAIMS,
	ATTR_HIBERNATION_SUPPORTED_STATES,
	ATTR_STARTER_ABILITY_LIST,
	ATTR_HAS_VM,
	ATTR_VM_GUEST_MAC,
	ATTR_VM_GUEST_IP,
	ATTR_VM_GUEST_MEM,
	ATTR_VM_AVAIL_NUM,
	ATTR_VM_MEMORY,
	ATTR_VM_NETWORKING,
	ATTR_VM_ALL_GUEST_IPS,
	ATTR_VM_ALL_GUEST_MACS,
	ATTR_VM_TYPE,
	ATTR_VM_NETWORKING_TYPES,
	ATTR_HAS_RECONNECT,
	ATTR_HAS_FILE_TRANSFER,
	ATTR_HAS_PER_FILE_ENCRYPTION,
	ATTR_HAS_MPI,
	ATTR_HAS_TDP,
	ATTR_HAS_JOB_DEFERRAL,
	ATTR_HAS_JIC_LOCAL_CONFIG,
	ATTR_HAS_JIC_LOCAL_STDIN,
	ATTR_HAS_JAVA,
	ATTR_HAS_WIN_RUN_AS_OWNER,
	ATTR_JAVA_VENDOR,
	ATTR_JAVA_VERSION,
	"JavaSpecificationVersion",
	ATTR_JAVA_MFLOPS,
	NULL		// list must end with NULL
};

static const char *job_attrs_list[]  = {
	ATTR_CLUSTER_ID,
	ATTR_PROC_ID,
	ATTR_Q_DATE,
	ATTR_COMPLETION_DATE,
	ATTR_OWNER,
	ATTR_NT_DOMAIN,
	ATTR_JOB_REMOTE_WALL_CLOCK,
	ATTR_JOB_LOCAL_USER_CPU,
	ATTR_JOB_LOCAL_SYS_CPU,
	ATTR_JOB_REMOTE_USER_CPU,
	ATTR_JOB_REMOTE_SYS_CPU,
	ATTR_NUM_CKPTS,
	ATTR_NUM_JOB_STARTS,
	ATTR_NUM_RESTARTS,
	ATTR_NUM_SYSTEM_HOLDS,
	ATTR_JOB_COMMITTED_TIME,
	ATTR_TOTAL_SUSPENSIONS,
	ATTR_LAST_SUSPENSION_TIME,
	ATTR_CUMULATIVE_SUSPENSION_TIME,
	ATTR_JOB_UNIVERSE,
	ATTR_JOB_CMD,
	ATTR_TRANSFER_EXECUTABLE,
	ATTR_WANT_REMOTE_SYSCALLS,
	ATTR_WANT_CHECKPOINT,
	ATTR_JOB_CMD_MD5,
	ATTR_JOB_CMD_HASH,
	ATTR_GRID_RESOURCE,
	ATTR_JOB_VM_TYPE,
	ATTR_JOB_VM_CHECKPOINT,
	ATTR_JOB_VM_NETWORKING,
	ATTR_JOB_VM_NETWORKING_TYPE,
	ATTR_JOB_VM_MEMORY,
	ATTR_JOB_VM_VCPUS,
	ATTR_JOB_VM_MACADDR,
	ATTR_JOB_VM_HARDWARE_VT,
	ATTR_WHEN_TO_TRANSFER_OUTPUT,
	ATTR_SHOULD_TRANSFER_FILES,
	ATTR_MIN_HOSTS,
	ATTR_MAX_HOSTS,
	ATTR_MACHINE_COUNT,
	ATTR_REQUEST_CPUS,
	ATTR_NEXT_JOB_START_DELAY,
	ATTR_IMAGE_SIZE,
	ATTR_EXECUTABLE_SIZE,
	ATTR_DISK_USAGE,
	ATTR_REQUEST_MEMORY,
	ATTR_REQUEST_DISK,
	ATTR_FILE_REMAPS,
	ATTR_BUFFER_FILES,
	ATTR_BUFFER_SIZE,
	ATTR_BUFFER_BLOCK_SIZE,
	ATTR_TRANSFER_INPUT_FILES,
	ATTR_TRANSFER_OUTPUT_FILES,
	ATTR_JAR_FILES,
	ATTR_JOB_INPUT,
	ATTR_JOB_OUTPUT,
	ATTR_JOB_ERROR,
	ATTR_TRANSFER_OUTPUT_REMAPS,
	ATTR_ENCRYPT_INPUT_FILES,
	ATTR_ENCRYPT_OUTPUT_FILES,
	ATTR_DONT_ENCRYPT_INPUT_FILES,
	ATTR_DONT_ENCRYPT_OUTPUT_FILES,
	ATTR_TRANSFER_FILES,
	ATTR_NEVER_CREATE_JOB_SANDBOX,
	ATTR_FETCH_FILES,
	ATTR_COMPRESS_FILES,
	ATTR_APPEND_FILES,
	ATTR_LOCAL_FILES,
	ATTR_JOB_JAVA_VM_ARGS1,
	ATTR_JOB_JAVA_VM_ARGS2,
	ATTR_PARALLEL_SCRIPT_SHADOW,
	ATTR_PARALLEL_SCRIPT_STARTER,
	ATTR_TRANSFER_INPUT,
	ATTR_STREAM_INPUT,
	ATTR_TRANSFER_OUTPUT,
	ATTR_STREAM_OUTPUT,
	ATTR_TRANSFER_ERROR,
	ATTR_STREAM_ERROR,
	ATTR_JOB_STATUS,
	ATTR_LAST_JOB_STATUS,
	ATTR_HOLD_REASON,
	ATTR_HOLD_REASON_CODE,
	ATTR_HOLD_REASON_SUBCODE,	
	ATTR_ENTERED_CURRENT_STATUS,
	ATTR_JOB_PRIO,
	ATTR_NICE_USER,
	ATTR_PERIODIC_HOLD_CHECK,
	ATTR_PERIODIC_RELEASE_CHECK,
	ATTR_PERIODIC_REMOVE_CHECK,
	ATTR_ON_EXIT_HOLD_CHECK,
	ATTR_JOB_LEAVE_IN_QUEUE,
	ATTR_ON_EXIT_REMOVE_CHECK,
	ATTR_JOB_NOOP,
	ATTR_JOB_NOOP_EXIT_SIGNAL,
	ATTR_JOB_NOOP_EXIT_CODE,
	ATTR_JOB_NOTIFICATION,
	ATTR_NOTIFY_USER,
	ATTR_EMAIL_ATTRIBUTES,
	ATTR_LAST_MATCH_LIST_LENGTH,
	ATTR_DAG_NODE_NAME,
	ATTR_DAGMAN_JOB_ID,
	ATTR_JOB_REMOTE_IWD,
	ATTR_JOB_ARGUMENTS1,
	ATTR_JOB_ARGUMENTS2,
	ATTR_DEFERRAL_TIME,
	ATTR_DEFERRAL_WINDOW,
	ATTR_DEFERRAL_PREP_TIME,
	ATTR_SCHEDD_INTERVAL,
	ATTR_JOB_ENVIRONMENT1,
	ATTR_JOB_ENVIRONMENT2,
	ATTR_JOB_ENVIRONMENT1_DELIM,
	ATTR_JOB_ROOT_DIR,
	ATTR_TOOL_DAEMON_CMD,
	ATTR_TOOL_DAEMON_INPUT,
	ATTR_TOOL_DAEMON_OUTPUT,
	ATTR_TOOL_DAEMON_ERROR,
	ATTR_TOOL_DAEMON_ARGS1,
	ATTR_TOOL_DAEMON_ARGS2,
	ATTR_SUSPEND_JOB_AT_EXEC,
	ATTR_JOB_RUNAS_OWNER,
	ATTR_JOB_LOAD_PROFILE,
	ATTR_JOB_IWD,
	ATTR_ULOG_FILE,
	ATTR_ULOG_USE_XML,
	ATTR_CORE_SIZE,
	ATTR_JOB_LEASE_DURATION,
	ATTR_GRID_JOB_ID,
	ATTR_JOB_MATCHED,
	ATTR_CURRENT_HOSTS,
	ATTR_GLOBUS_RESUBMIT_CHECK,
	ATTR_USE_GRID_SHELL,
	ATTR_REMATCH_CHECK,
	ATTR_GLOBUS_RSL,
	ATTR_GLOBUS_XML,
	ATTR_NORDUGRID_RSL,
	ATTR_KEYSTORE_ALIAS,
	ATTR_KEYSTORE_PASSPHRASE_FILE,
	ATTR_AMAZON_PUBLIC_KEY,
	ATTR_AMAZON_PRIVATE_KEY,
	ATTR_AMAZON_KEY_PAIR_FILE,
	ATTR_AMAZON_SECURITY_GROUPS,
	ATTR_AMAZON_AMI_ID,
	ATTR_AMAZON_INSTANCE_TYPE,
	ATTR_AMAZON_USER_DATA,
	ATTR_AMAZON_USER_DATA_FILE,
	ATTR_X509_USER_PROXY_SUBJECT,
	ATTR_X509_USER_PROXY,
	ATTR_X509_USER_PROXY_VONAME,
	ATTR_X509_USER_PROXY_FIRST_FQAN,
	ATTR_X509_USER_PROXY_FQAN,
	ATTR_MYPROXY_HOST_NAME,
	ATTR_MYPROXY_SERVER_DN,
	ATTR_MYPROXY_PASSWORD,
	ATTR_MYPROXY_CRED_NAME,
	ATTR_MYPROXY_REFRESH_THRESHOLD,
	ATTR_MYPROXY_NEW_PROXY_LIFETIME,
	ATTR_MYPROXY_PASSWORD_EXISTS,
	ATTR_KILL_SIG,
	ATTR_REMOVE_KILL_SIG,
	ATTR_HOLD_KILL_SIG,
	ATTR_CONCURRENCY_LIMITS,
	ATTR_REMOVE_REASON,
	ATTR_USER,
	ATTR_SHADOW_BIRTHDATE,
	ATTR_JOB_FINISHED_HOOK_DONE,
	ATTR_STAGE_OUT_FINISH,
	ATTR_STAGE_IN_START,
	ATTR_STAGE_OUT_START,
	ATTR_JOB_STATUS_ON_RELEASE,
	ATTR_LAST_REJ_MATCH_REASON,
	ATTR_LAST_REJ_MATCH_TIME,
	ATTR_LAST_MATCH_TIME,
	ATTR_NUM_MATCHES,
	ATTR_LAST_JOB_LEASE_RENEWAL,
	ATTR_JOB_START_DATE,
	ATTR_JOB_LAST_START_DATE,
	ATTR_JOB_CURRENT_START_DATE,
	ATTR_JOB_RUN_COUNT,
	ATTR_NUM_SHADOW_STARTS,
	ATTR_NUM_RESTARTS,
	ATTR_REMOTE_HOST,
	ATTR_REMOTE_SLOT_ID,
	ATTR_REMOTE_POOL,
	ATTR_STARTD_PRINCIPAL,
	ATTR_JOB_WALL_CLOCK_CKPT,
	ATTR_LAST_REMOTE_HOST,
	ATTR_LAST_PUBLIC_CLAIM_ID,
	ATTR_ORIG_MAX_HOSTS,
	ATTR_NUM_SHADOW_EXCEPTIONS,
	ATTR_JOB_EXIT_STATUS,
	ATTR_ON_EXIT_CODE,
	ATTR_ON_EXIT_BY_SIGNAL,
	ATTR_ON_EXIT_SIGNAL,
	ATTR_RELEASE_REASON,
	ATTR_GRID_JOB_ID,
	NULL		// list must end with NULL
};

static void InitTargetAttrLists()
{
	const char **attr;
	char *tmp;
	MyString buff;
	StringList tmp_strlist;

	///////////////////////////////////
	// Set up Machine attributes list
	///////////////////////////////////
	for ( attr = machine_attrs_list; *attr; attr++ ) {
		machine_attrs_ad.AssignExpr( *attr, "True" );
	}

	machine_attrs_ad.Delete( ATTR_CURRENT_TIME );

	tmp = param( "STARTD_EXPRS" );
	if ( tmp ) {
		tmp_strlist.initializeFromString( tmp );
		free( tmp );
		tmp_strlist.rewind();
		while ( (tmp = tmp_strlist.next()) ) {
			machine_attrs_ad.AssignExpr( tmp, "True" );
		}
		tmp_strlist.clearAll();
	}

	tmp = param( "STARTD_ATTRS" );
	if ( tmp ) {
		tmp_strlist.initializeFromString( tmp );
		free( tmp );
		tmp_strlist.rewind();
		while ( (tmp = tmp_strlist.next()) ) {
			machine_attrs_ad.AssignExpr( tmp, "True" );
		}
		tmp_strlist.clearAll();
	}

	tmp = param( "STARTD_RESOURCE_PREFIX" );
	if ( tmp ) {
		buff.sprintf( "%s*", tmp );
		machine_attrs_strlist.append( buff.Value() );
		free( tmp );
	} else {
		machine_attrs_strlist.append( "slot*" );
	}

	tmp = param( "TARGET_MACHINE_ATTRS" );
	if ( tmp ) {
		machine_attrs_strlist.initializeFromString( tmp );
		free( tmp );
	}

	///////////////////////////////////
	// Set up Job attributes list
	///////////////////////////////////
	for ( attr = job_attrs_list; *attr; attr++ ) {
		job_attrs_ad.AssignExpr( *attr, "True" );
	}

	job_attrs_ad.Delete( ATTR_CURRENT_TIME );

	tmp = param( "SUBMIT_EXPRS" );
	if ( tmp ) {
		tmp_strlist.initializeFromString( tmp );
		free( tmp );
		tmp_strlist.rewind();
		while ( (tmp = tmp_strlist.next()) ) {
			job_attrs_ad.AssignExpr( tmp, "True" );
		}
		tmp_strlist.clearAll();
	}

	buff.sprintf( "%s*", ATTR_LAST_MATCH_LIST_PREFIX );
	job_attrs_strlist.append( buff.Value() );

	buff.sprintf( "%s*", ATTR_NEGOTIATOR_MATCH_EXPR );
	job_attrs_strlist.append( buff.Value() );

	tmp = param( "TARGET_JOB_ATTRS" );
	if ( tmp ) {
		job_attrs_strlist.initializeFromString( tmp );
		free( tmp );
	}

	target_attrs_init = true;
}

classad::ExprTree *AddTargetRefs( classad::ExprTree *tree, TargetAdType target_type )
{
	if ( !target_attrs_init ) {
		InitTargetAttrLists();
	}

	if( tree == NULL ) {
		return NULL;
	}
	if ( target_type != TargetMachineAttrs && target_type != TargetJobAttrs ) {
		return NULL;
	}
	classad::ExprTree::NodeKind nKind = tree->GetKind( );
	switch( nKind ) {
	case classad::ExprTree::ATTRREF_NODE: {
		classad::ExprTree *expr = NULL;
		string attr = "";
		bool abs = false;
		( ( classad::AttributeReference * )tree )->GetComponents(expr,attr,abs);
		if( abs || expr != NULL ) {
			return tree->Copy( );
		}
		else {
			bool add_target = false;
			if ( target_type == TargetMachineAttrs ) {
				if ( machine_attrs_ad.Lookup( attr.c_str() ) ||
					 machine_attrs_strlist.contains_anycase_withwildcard( attr.c_str() ) ) {
					add_target = true;
				}
			} else {
				if ( job_attrs_ad.Lookup( attr.c_str() ) ||
					 job_attrs_strlist.contains_anycase_withwildcard( attr.c_str() ) ) {
					add_target = true;
				}
			}
			if( add_target ) {
					// attribute is in our list, so insert "target"
				classad::AttributeReference *target = NULL;
				target = classad::AttributeReference::MakeAttributeReference(NULL,
																	"target");
				return classad::AttributeReference::MakeAttributeReference(target,attr);
			}
			else {
				return tree->Copy( );
			}
		}
	}
	case classad::ExprTree::OP_NODE: {
		classad::Operation::OpKind oKind;
		classad::ExprTree * expr1 = NULL;
		classad::ExprTree * expr2 = NULL;
		classad::ExprTree * expr3 = NULL;
		classad::ExprTree * newExpr1 = NULL;
		classad::ExprTree * newExpr2 = NULL;
		classad::ExprTree * newExpr3 = NULL;
		( ( classad::Operation * )tree )->GetComponents( oKind, expr1, expr2, expr3 );
		if( expr1 != NULL ) {
			newExpr1 = AddTargetRefs( expr1, target_type );
		}
		if( expr2 != NULL ) {
			newExpr2 = AddTargetRefs( expr2, target_type );
		}
		if( expr3 != NULL ) {
			newExpr3 = AddTargetRefs( expr3, target_type );
		}
		return classad::Operation::MakeOperation( oKind, newExpr1, newExpr2, newExpr3 );
	}
	case classad::ExprTree::FN_CALL_NODE: {
		std::string fn_name;
		classad::ArgumentList old_fn_args;
		classad::ArgumentList new_fn_args;
		( ( classad::FunctionCall * )tree )->GetComponents( fn_name, old_fn_args );
		for ( classad::ArgumentList::iterator i = old_fn_args.begin(); i != old_fn_args.end(); i++ ) {
			new_fn_args.push_back( AddTargetRefs( *i, target_type ) );
		}
		return classad::FunctionCall::MakeFunctionCall( fn_name, new_fn_args );
	}
	default: {
 			// old ClassAds have no nested ClassAds or lists
			// literals have no attrrefs in them
		return tree->Copy( );
	}
	}
}

const char *ConvertEscapingOldToNew( const char *str )
{
	static std::string new_str;

	new_str = "";

		// String escaping is different between new and old ClassAds.
		// We need to convert the escaping from old to new style before
		// handing the expression to the new ClassAds parser.
	for ( int i = 0; str[i] != '\0'; i++ ) {
		if ( str[i] == '\\' && 
			 ( str[i + 1] != '"' ||
			   str[i + 1] == '"' &&
			   ( str[i + 2] == '\0' || str[i + 2] == '\n' ||
				 str[i + 2] == '\r') ) ) {
			new_str.append( 1, '\\' );
		}
		new_str.append( 1, str[i] );
	}

	return new_str.c_str();
}

// end functions

} // namespace compat_classad
