/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#ifndef _nkadaptor_h
#define _nkadaptor_h

/**
 * New kernel adaptor.
 * This file usually contains definitions that allow upgrading mvstore client code
 * to a newer kernel, while preserving the ability to build against a previous kernel,
 * based on MVSTORE_IFACE_VER.
 */

namespace MVStore { typedef Value PropValue; }
#define INITLOCALPID(PID) (PID).ident = STORE_OWNER
#define LOCALPID(PID) (PID).pid
#define SETVATTR(vs, propid, op) \
	{ (vs).setPropID(propid); (vs).setOp(op); }
#define SETVATTR_C(vs, propid, op, elementid) \
	{ SETVATTR(vs, propid, op) (vs).eid = elementid; }
#define SETVALUE(vs, propid, value, op) \
	{ (vs).set(value); SETVATTR(vs, propid, op) }
#define SETVALUE_C(vs, propid, value, op, elementid) \
	{ (vs).set(value); SETVATTR_C(vs, propid, op, elementid) }

#ifdef WIN32
	#define INTERLOCKEDI(val) InterlockedIncrement(val)
	#define INTERLOCKEDD(val) InterlockedDecrement(val)
#else
	#define INTERLOCKEDI(val) InterlockedIncrement(val)
	#define INTERLOCKEDD(val) InterlockedDecrement(val)
#endif

	#define STARTUP_DELETE_LOG_ON_SHUTDOWN 0

	#define setConditionFT addConditionFT

#endif
