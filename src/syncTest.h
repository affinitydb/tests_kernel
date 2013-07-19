/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

Written by Mark Venguerov, Max Windish, Michael Andronov 2004 - 2010

**************************************************************************************/
#ifndef _SYNC_TEST_H_
#define _SYNC_TEST_H_

#include "sync.h"
#include "stdafx.h"

using namespace MVTestsPortability;

inline void RWLock::wait(SemData *q) {
	assert(ptrdiff_t(queue)==ptrdiff_t(~0ULL) && ptrdiff_t(q)!=ptrdiff_t(~0ULL));
	SemData sd,&sem=sd;
	sem.next=q; queue=&sem; sem.wait(); sem.next=NULL;
}

#endif
