/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#ifndef _mvstoreex_mvauto_h
#define _mvstoreex_mvauto_h
//
// Helpful code for dealing with MV Store resources
//

/**
 * Auto MvStore pointer.  This object takes ownership of the given MvStore
 * pointer and automatically destroy()s it when the object's destructor is
 * called.  The concept is similar to the standard C++ auto_ptr<> template.
 * 
 * Example usages:
 *
 *  CmvautoPtr<IPIN> pin1( mSession->getPIN( pid ) );
 *  if ( pin1.IsValid() ) { 
 *      const Value * v = pin1->getValue( propX ) ; ... }
 * 
 *and
 *
 *  CmvautoPtr<IDumpStore> dump ;
 *  mSession->dumpStore( dump.Get(), false ) ;
 */
template<class T>
class CmvautoPtr
{
    T* m_p;

    // Not implemented, unless needed
	CmvautoPtr<T>& operator=( T* p ) ;

public:

    /**
     * Construct an auto pointer for the given store object.
     */
    explicit CmvautoPtr( T* p /**< pointer to take ownership of */
        ) : m_p( p ) {}

	/**
	* Construct with an existing autopointer
	* Call this variation with caution because
	* ownership must be taken from the original AutoPtr.
	*/

	CmvautoPtr( CmvautoPtr<T>& other ) 
	{
		m_p = other.Detach() ; 
	}


    /**
     * Construct an auto pointer not attached to any store object yet.
     */
    CmvautoPtr() : m_p( 0 ) {}

    ~CmvautoPtr()
    {
        if( 0 != m_p ) m_p->destroy();
    }

    /**
     * Attach this object to the new pointer, and detaching it from the
     * previous one.  The previous pointer is not destroyed.
     */
    void Attach( T* p )
    {
        Detach();

        m_p = p;
    }

    /**
     * Detach this object from its internal pointer without destroying it.
     *
     * @return The pointer that was detached
     */
    T* Detach()
    {
        T* pOld = m_p;

        m_p = 0;

        return pOld;
    }

    /**
     * Detach this object from its internal pointer and destroy it.
     */
    void Destroy()
    {
        if( 0 != m_p ) m_p->destroy();

        m_p = 0;
    }

    /**
     * Check if this auto pointer is attached to a pointer.
     *
     * @return True if the auto pointer is attached, false otherwise
     */
    bool IsValid() const { return 0 != m_p; }

	T*& Get() { return m_p; } // For signatures like ISession::dumpStore

    T* operator->() const { return m_p; }

    operator T*() { return m_p; }

    operator T**() { Destroy(); return &m_p; }

    /**
    * With assignment operator, ownership must be
    * taken from the original.
    **/
    CmvautoPtr<T>& operator=( CmvautoPtr<T>& other )
	{
		Detach() ;
		m_p = other.Detach() ;
		return *this ;
	}

};


// Same thing but for classes that use "Destroy" rather than "destroy" for their destruction
// (This is not needed for any store stuff only objects elsewhere in the tree)
// REVIEW: Maybe there is some fancy template mechanism to avoid the code duplication here?
template<class T>
class CmvautoPtrD
{
    T* m_p;
	CmvautoPtrD<T>& operator=( T* p ) ;
public:
    explicit CmvautoPtrD( T* p ) : m_p( p ) {}
	CmvautoPtrD( CmvautoPtrD<T>& other ) { m_p = other.Detach() ; }
    CmvautoPtrD() : m_p( 0 ) {}
    ~CmvautoPtrD(){ if( 0 != m_p ) m_p->Destroy(); }
    void Attach( T* p )
    {
        Detach();
        m_p = p;
    }
    T* Detach()
    {
        T* pOld = m_p;
        m_p = 0;
        return pOld;
    }
    void Destroy()
    {
        if( 0 != m_p ) m_p->Destroy();
        m_p = 0;
    }
    bool IsValid() const { return 0 != m_p; }
	T*& Get() { return m_p; } 
	T* operator->() const { return m_p; }
    operator T*() { return m_p; }
    operator T**() { Destroy(); return &m_p; }
    CmvautoPtrD<T>& operator=( CmvautoPtrD<T>& other )
	{
		Detach() ;
		m_p = other.Detach() ;
		return *this ;
	}
};

// Same thing but for classes using Release() rather than destroy()
template<class T>
class CmvautoPtrR
{
    T* m_p;
	CmvautoPtrR<T>& operator=( T* p ) ;
public:
    explicit CmvautoPtrR( T* p ) : m_p( p ) {}
	CmvautoPtrR( CmvautoPtrR<T>& other ) { m_p = other.Detach() ; }
    CmvautoPtrR() : m_p( 0 ) {}
    ~CmvautoPtrR(){ if( 0 != m_p ) m_p->Release(); }
    void Attach( T* p )
    {
        Detach();
        m_p = p;
    }
    T* Detach()
    {
        T* pOld = m_p;
        m_p = 0;
        return pOld;
    }
    void Destroy()
    {
        if( 0 != m_p ) m_p->Release();
        m_p = 0;
    }
    bool IsValid() const { return 0 != m_p; }
	T*& Get() { return m_p; } 
	T* operator->() const { return m_p; }
    operator T*() { return m_p; }
    operator T**() { Destroy(); return &m_p; }
    CmvautoPtrR<T>& operator=( CmvautoPtrR<T>& other )
	{
		Detach() ;
		m_p = other.Detach() ;
		return *this ;
	}
};

#endif
