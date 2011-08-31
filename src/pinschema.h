/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#ifndef _PINSCHEMA_H
#define _PINSCHEMA_H

#include "app.h"
#include "tests.h"
#include "mvauto.h"
#include <map>

// Schema Flags.  They control meta field of Value
// and other decisions
#define SF_COLLECTION   0x01
#define SF_FAMILY		0x02
#define SF_FT			0x04  // Full text index (e.g. opposite of META_PROP_NOFTINDEX)
#define SF_SSV          0x08  // equiv to META_PROP_SSTORAGE
#define SF_NOSTOPWORDS  0x10  // META_PROP_STOPWORDS

struct PropInfo
{
	// Represent a registered property in the store
	// and rules about the data that it contains

	PropInfo()
		: id(STORE_INVALID_PROPID)
		, index(0)
		, type(VT_ANY)
		, schemaFlags(0)
		, indexOp(OP_IN)
	{
	}

	bool hasFlag( uint16_t inFlag ) { return (( schemaFlags & inFlag ) == inFlag ) ; } 

	uint8_t meta()
	{
		uint8_t meta = 0 ;
		if ( !hasFlag( SF_FT ) ) meta |= META_PROP_NOFTINDEX ;
		if ( hasFlag( SF_SSV ) ) meta |= META_PROP_SSTORAGE ;
		if ( hasFlag( SF_NOSTOPWORDS ) ) meta |= META_PROP_STOPWORDS ;
		return meta ;
	}

public:
	PropertyID id ;
	size_t index ;  // insertion order of this property (e.g. a pin creation) 
					// e.g. this is like a column index

	std::string URIName ; // Property name, the primary symbolic name to
						   // use in the testing.

	uint8_t type ;	// Expected underlying type (perhaps just a hint?)
	uint16_t schemaFlags ; // bits from SF_*

	ExprOp indexOp ; // Op for family OP_IN, OP_EQ etc (if SF_FAMILY set)
	ClassID cid ;	 // Class ID (if SF_FAMILY set) 
} ;

class PinSchema
{
	// Base class to help construct and manipulate pins
	// with a specific definition.  Classes will be derived from this 
	// one to take advantage of its services
public:	

	PinSchema( ISession* inSession ) 
		: mSession( inSession)
	{
	}

	void addProperty( 
		const char * inPropName, 
		uint8_t inType, 
		uint16_t inSchemaFlags, 
		ExprOp inFamilyOp = OP_IN )
	{
		PropInfo newProp ;
		newProp.index = mProps.size() ;
		newProp.URIName = inPropName ;
		newProp.id = MVTApp::getProp( mSession, inPropName ) ;
		newProp.schemaFlags = inSchemaFlags ;
		newProp.type = inType ;
		newProp.indexOp = inFamilyOp ;

		mProps.push_back( newProp ) ;
	}

	void postAddProps()
	{
		// Once all properties added, build map 
		size_t i ;
		for ( i = 0 ; i <  mProps.size() ; i++ )
		{
			PropInfo * prop = &(mProps[i]) ;
			// Build map by property name for fast lookup
			mPropMap.insert( TPropDict::value_type(prop->URIName, prop ));
		}

		// Build auto-generated families (other, more complex
		// families would need to be generated by derived classes)
		//
		// This code can handle families of the sort that correspond
		// to a direct indexed lookup by property value
		// 
		//mv:defineclass("EntityIDFamily($var)","/pin[EntityID = $var]",0)
		// or 
		//mv:defineclass("EntityIDFamily($var)","/pin[EntityID in $var]",0)

		for ( i = 0 ; i <  mProps.size() ; i++ )
		{
			PropInfo * prop = &(mProps[i]) ;

			if ( !prop->hasFlag( SF_FAMILY ) )
			{
				continue ;
			}
			// Naming convention based on indexed property
			string className( prop->URIName ) ;
			className += "Family" ; 

			RC rc = mSession->getClassID( className.c_str(), prop->cid ) ;
			if ( rc == RC_OK )
			{
				// Class already registered.  (Hopefully 
				// existing class has exactly the same name 
				// as the one we wanted to register!)
				continue ;
			}

			CmvautoPtr<IStmt> lClassQ( mSession->createStmt() ) ;
			QVarID lVar = lClassQ->addVariable();
			Value lV[2];
			lV[0].setVarRef(lVar,(prop->id));
			lV[1].setParam(0);
			CmvautoPtr<IExprTree> lET( mSession->expr(prop->indexOp, 2, lV, 0));

			rc = lClassQ->addCondition(lVar,lET);	assert( rc == RC_OK ) ;
			rc = ITest::defineClass(mSession,className.c_str(), lClassQ, &prop->cid);  assert( rc == RC_OK ) ;

			// REVIEW: Should any notification be registered?
		}		
	}

	PropInfo* getPropInfo( const char * inPropName )
	{
		TPropDict::iterator it = mPropMap.find( inPropName ) ;

		if ( it == mPropMap.end() )
			return NULL ;

		return (*it).second ;
	}

	// Shortcuts to common attributes
	PropertyID prop( const char * inPropName )
	{
		PropInfo * info = getPropInfo( inPropName ) ;
		if ( info == NULL ) { assert(false) ; return STORE_INVALID_PROPID ; }
		return info->id ;
	}

	uint8_t meta( const char * inPropName )
	{
		PropInfo * info = getPropInfo( inPropName ) ;
		if ( info == NULL ) { assert(false) ; return 0 ; }
		return info->meta() ;
	}
	size_t propIndex( const char * inPropName )
	{
		PropInfo * info = getPropInfo( inPropName ) ;
		if ( info == NULL ) { assert(!"Invalid property name") ; return 0 ;	}
		return info->index ;
	}

	void setSession( ISession* inSession ) 
	{
		mSession = inSession ;
	}

	size_t numProps() { return mProps.size() ; }

	bool isValid()
	{
		if ( mProps.empty() ) return false ; // addProperty not called
		if ( mPropMap.size() != mProps.size() ) return false ; // postInitValues not called
		return true ;
	}

public:
	// Definition of properties, in the order that they are inserted
	std::vector<PropInfo> mProps ;

protected:
	// Accelerated lookup by property name
	// Map points to PropInfo objects owned by mProps.
	typedef std::map<std::string,PropInfo*> TPropDict ;
	TPropDict mPropMap ;

	// Warning this has implications for multi-threaded
	// usage.  Perhaps it should only be used during schema
	// definition time?  Or not belong here at all
	ISession * mSession ;

private:
	PinSchema() ;
	PinSchema(const PinSchema &) ;
} ;


class TypedPinCreator
{
public:
	// Use this class to build a pin based on a specific PinSchema
	// It takes care of setting all the flags and 
	// the type conversions needed to set data on the pin
	TypedPinCreator( ISession * inSession, PinSchema * inSchema )
		: mVals(NULL)
		, mValPos(0)
		, mSession( inSession )
		, mSchema( inSchema )
	{
		assert( mSchema->isValid() ) ;
		init() ;
	}

	~TypedPinCreator()
	{
		term() ;
	}

	//
	// Services for initializing a Value array that matches
	// the PinSchema (e.g. for a Pin Creation call)
	// 
	void init()
	{
		assert( mVals == NULL ) ;

		mVals = (Value*)mSession->alloc(sizeof(Value) * mSchema->numProps()) ;
		mValPos = 0 ;
	}

	void destroy()
	{
		delete this ;
	}

	void detach()
	{
		// For the case of createUncommittedPINs is being used without MODE_COPY_VALUES
		// In that case ownership of memory has to pass to store
		mVals = NULL ;
		mValPos = 0 ;
	}

	void term()
	{
		if ( mVals != NULL )
		{
			assert(!"Not fully implemented - leaking what values point to") ;

			mSession->free( mVals ) ;
			mVals = NULL ;
			mValPos = 0 ;
		}
	}

	bool add( const char * inPropName, const string& inVal )
	{
		assert( mValPos < mSchema->numProps() ) ;
		assert( mVals != NULL ) ;

		if ( inVal.empty() )
		{
			// REVIEW: not storing empty strings
			return true ;
		}

		char * copy = (char*) mSession->alloc( inVal.size() + 1 ) ;
		memcpy( copy, inVal.c_str(), inVal.size() + 1 ) ;

		mVals[mValPos].set( copy ) ;

		return prepVal(inPropName) ;
	}

	bool add( const char * inPropName, int inVal )
	{
		assert( mValPos < mSchema->numProps() ) ;
		assert( mVals != NULL ) ;
		mVals[mValPos].set( inVal ) ;

		return prepVal(inPropName) ;
	}

	bool add( const char * inPropName, const vector<string> & inStrs )
	{
		assert( mValPos < mSchema->numProps() ) ;
		assert( mVals != NULL ) ;
		if( inStrs.empty() )
		{
			return true ; // Empty collection represented by missing property
		}

		Value * arrayVals = (Value*)mSession->alloc(sizeof(Value) * inStrs.size() ) ;	

		for ( size_t i = 0 ; i < inStrs.size() ; i++ )
		{
			const string & str = inStrs[i] ;
			char * copy = (char*) mSession->alloc( str.size() + 1 ) ;
			memcpy( copy, str.c_str(), str.size() + 1 ) ;
			arrayVals[i].set(copy) ;
		}

		mVals[mValPos].set( arrayVals, (unsigned int) inStrs.size() ) ;

		return prepVal(inPropName) ;
	}

	bool addDateTime( const char * inPropName, uint64_t inDT )
	{
		assert( mValPos < mSchema->numProps() ) ;
		assert( mVals != NULL ) ;
		mVals[mValPos].setDateTime( inDT ) ;

		return prepVal(inPropName) ;
	}

protected:

    bool prepVal(const char * inPropName)
	{
		PropInfo * info = mSchema->getPropInfo( inPropName ) ;
		if ( info == NULL ) { assert( false ) ; return false ; }

		if (!testType( &(mVals[mValPos]), info )) { assert( !"Invalid type" ) ; return false ; }
		
		mVals[mValPos].setPropID( info->id ) ;
		mVals[mValPos].setMeta( info->meta() ) ;
		mValPos++ ;

		return true ;
	}

	bool isString( uint8_t inType )
	{
		switch( inType )
		{
		case( VT_STRING ):
		case( VT_BSTR ):
		case( VT_URL ):
			return true ;
		default: break ;
		}
		return false ;
	}

	bool isNum( uint8_t inType )
	{
		switch( inType )
		{
		case(VT_ENUM ):
		case(VT_INT):
		case(VT_UINT):
		case(VT_INT64):
		case(VT_UINT64):
		case(VT_FLOAT):
		case(VT_DOUBLE):
			return true ;
		default: break ;
		}
		return false ;
	}

	bool testType( const Value * inVal, PropInfo * inInfo )
	{
		// Check if types are equivalent
		// Direct comparison is too strict

		uint8_t valType = inVal->type ;
		uint8_t expectedType = inInfo->type ;

		if ( valType == VT_ARRAY )
		{
			if ( inVal->length == 0 ) { assert( false ) ; return false ; }

			if ( 0 == ( inInfo->schemaFlags & SF_COLLECTION ) )
			{
				// Schema doesn't expect collection here
				// (note: we can't do corresponding opposite check)
				return false ; 
			}

			valType = inVal->varray[0].type ;
		}
		else if ( valType == VT_COLLECTION )
		{
			assert( !"Not implemented yet" ) ;
			return true ;
		}

		if ( isString( expectedType ) )
		{
			return isString( valType ) ;
		}
		else if ( isNum( expectedType ) )
		{
			return isNum( valType ) ;
		}
		else if ( expectedType == VT_DATETIME )
		{
			return ( valType == VT_DATETIME ) ;
		}
		else if ( valType == VT_ERROR )
		{
			return false ; // not initialized
		}
		else
		{
			// Not implemented yet 
			assert( !"Not implemented yet" ) ;
			return true ;
		}
	}
public:

	Value * mVals ;
	unsigned int mValPos ;
protected:
	ISession * mSession ;
	PinSchema * mSchema ; 
} ;

#endif