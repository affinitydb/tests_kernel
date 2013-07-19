/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

//
// Mvstore Kernel API
//
// This header exposes the exported header files of the kernel.
//
// It also hides the details of the promoted/unpromoted kernel 
// (controlled by the USE_KERNEL_VERSION define)
// 
// Directly including kernel headers is dangerous and should be avoided.
// e.g. other headers should be considered to contain private data structures.
//
// For sync.h use commons/mvcore/sync.h instead
// For types.h use commons/mvcore/sync.h instead
//
// The only other heads from kernel are some specialized interfaces.
// They are: storeins.h, storeio.h
//

#ifndef __MVSTOREAPI_H


// ifacever.h is important for doing conditional defines as kernel API changes
#include "../../kernel/include/ifacever.h"

// Error codes (included from kernel headers but
// listing here to be explicit that it is official part of interface)
#include "../../kernel/include/rc.h"
		
// Main mvstore API
#include "../../kernel/include/affinity.h"

// notification structures etc
#include "../../kernel/include/startup.h"
	
// help with version changes, e.g. temporary transition help 
// as method get renamed	
#include "nkadaptor.h"

#endif
