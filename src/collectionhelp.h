/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#ifndef _COLLECTIONHELP_H
#define _COLLECTIONHELP_H

// Contains CollectionIterator and CollectionScanner

namespace MvStoreEx
{

	class CollectionIterator
	{
		// Helper class for tests for dealing with different 
		// possible store API representations of a collections
		// Those include 
		// -missing property (empty collection)
		// -single value (1 element collection)
		// -VT_COLLECTION
		//
		// WARNING: this object is only valid for as long as the
		// PIN memory is valid.  Any change to the PIN or property can
		// invalidate this class.
		//
		// NOTE: this is implemented entirely in the header file so no
		// mvstoreex dependency is necessary
		// 
		// History: Stolen from SAPI implementation!
	public:
		CollectionIterator( Afy::IPIN* in_Pin, Afy::PropertyID in_Prop )
			: m_value( NULL ), m_index(0), m_size(0)
		{
			if ( in_Pin )
			{
				m_value = in_Pin->getValue( in_Prop ) ;
			}
			Init() ;
		}
		CollectionIterator( Afy::Value const *in_value )
			: m_value( in_value ), m_index(0), m_size(0)
		{
			Init() ;
		}
		void Init()
		{
			if ( m_value == NULL )
			{
				// This can be considered a valid 
				// representation of an empty collection 
				m_size = 0 ;
			}
			else
			{
				m_size = m_value->count();
			}
		}
		uint32_t getSize( void )
		{
			return m_size;
		}
		Afy::Value const* getFirst( void )
		{
			m_index = 0;

			if ( m_value == NULL ) return NULL ;

			if ( m_value->type == Afy::VT_COLLECTION )
			{
				return m_value->isNav() ? m_value->nav->navigate( Afy::GO_FIRST ) : m_value->varray;
			}
			return m_value;
		}
		Afy::Value const* getLast( void )
		{
			if ( m_value == NULL ) return NULL ;

			m_index = m_size -1 ;

			if ( m_value->type == Afy::VT_COLLECTION )
			{
				return m_value->isNav() ? m_value->nav->navigate( Afy::GO_LAST ) : m_value->varray + m_size - 1;
			}
			return m_value;
		}
		Afy::Value const* getNext( void )
		{
			++m_index ;
			if ( m_value == NULL ) return NULL ;

			if ( m_value->type == Afy::VT_COLLECTION )
			{
				return m_value->isNav() ? m_value->nav->navigate( Afy::GO_NEXT ) : m_index < m_size ? m_value->varray + m_index : NULL;
			}
			return NULL;
		}
		Afy::Value const* getElementId( Afy::ElementID in_id )
		{
			if ( m_value == NULL ) return NULL ;

			if ( in_id == STORE_LAST_ELEMENT )
				return getLast() ;

			if ( in_id == STORE_FIRST_ELEMENT )
				return getFirst() ;

			if ( m_value->type == Afy::VT_COLLECTION )
			{
				// m_index is no longer valid!
				if (m_value->isNav()) return m_value->nav->navigate( Afy::GO_FINDBYID, in_id );
				
				for ( uint32_t i=0; i < m_size; ++i )
				{
					if ( m_value->varray[i].eid == in_id )
					{
						m_index = i ;
						return m_value->varray + i;
					}
				}
				return NULL;
			}

			if ( in_id == STORE_COLLECTION_ID ||
				m_value->eid == in_id ) 
			{
				
				return m_value;
			}

			return NULL;
		}

		uint32_t getCurrentPos() {
			return m_index ; 
		}

	private:
		CollectionIterator() {;}

	protected:
		Afy::Value const *m_value;
		uint32_t m_index;
		uint32_t m_size;
	};
} ;
#endif
