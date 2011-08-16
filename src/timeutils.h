/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#ifndef  __TIME_UTILS__
#define  __TIME_UTILS__

#include<string>
#include "types.h"

namespace mvcore
{
	class TimeUtils
	{
	public:
		static uint64_t getCurrentTime();
	};
};

#endif

