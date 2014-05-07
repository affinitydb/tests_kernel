/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "util.h"
#include "app.h"
#include "md5stream.h"
#include "mvauto.h"
namespace AfyKernel { typedef uint8_t FileID; };

//#define STORE_DYNAMIC_LINK
//#include "localbuilder.h"

#if defined(WIN32)
	#include <shellapi.h>
	#include <sys/timeb.h>		
	#include <direct.h>
	#ifdef _DEBUG
		#include <crtdbg.h>
		#define new new(_NORMAL_BLOCK, __FILE__, __LINE__)
	#endif
#else
	#include <fcntl.h>
	#include <dlfcn.h>
	#include <signal.h>
	#include <sys/wait.h>
	#include <sys/time.h>
	#include <sys/stat.h>
#endif

/*
int MVTUtil::executeStoreDoctor(
	char const * pArgs,
	char const * pOutputFile,
	bool bVerbose )
{
	REVIEW: perhaps possible to launch as separate process, but how to capture output
	and how to ease debugging

	const char * args = (pArgs==NULL)?"-all":pArgs;
	const char * outfile = (pOutputFile==NULL)?"sdrcl.txt":pOutputFile;

	string cmd = "sdrcl " + string(args) + string(" > ") + string(outfile) ; 

	#ifdef WIN32
	string exe = "cmd.exe"
	string prefix = "/C" ;
	executeProcess( exe, prefix+cmd, 0, 0, false, bVerbose );
	#else
	system( string("bash -c")
	#endif
}

*/
namespace utf {
/*
 * Check comments for utf namespace within util.h 
 */
const byte UTF8::slen[] = 
{
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,	// 0x00-0x0F
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,	// 0x10-0x1F 
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,	// 0x20-0x2F 
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,	// 0x30-0x3F 
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,	// 0x40-0x4F 
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,	// 0x50-0x5F 
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,	// 0x60-0x6F 
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,	// 0x70-0x7F 
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,	// 0x80-0x8F 
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,	// 0x90-0x9F 
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,	// 0xA0-0xAF 
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,	// 0xB0-0xBF 
	0,0,2,2,2,2,2,2,2,2,2,2,2,2,2,2,	// 0xC0-0xCF 
	2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,	// 0xD0-0xDF 
	3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,	// 0xE0-0xEF 
	4,4,4,4,4,0,0,0,0,0,0,0,0,0,0,0,	// 0xF0-0xFF 
};

const byte UTF8::sec[64][2] = 
{
	{ 0x00, 0x00 }, { 0x00, 0x00 }, { 0x80, 0xBF }, { 0x80, 0xBF },
	{ 0x80, 0xBF }, { 0x80, 0xBF }, { 0x80, 0xBF }, { 0x80, 0xBF },
	{ 0x80, 0xBF }, { 0x80, 0xBF }, { 0x80, 0xBF }, { 0x80, 0xBF },
	{ 0x80, 0xBF }, { 0x80, 0xBF }, { 0x80, 0xBF }, { 0x80, 0xBF },
	{ 0x80, 0xBF }, { 0x80, 0xBF }, { 0x80, 0xBF }, { 0x80, 0xBF },
	{ 0x80, 0xBF }, { 0x80, 0xBF }, { 0x80, 0xBF }, { 0x80, 0xBF },
	{ 0x80, 0xBF }, { 0x80, 0xBF }, { 0x80, 0xBF }, { 0x80, 0xBF }, 
	{ 0x80, 0xBF }, { 0x80, 0xBF }, { 0x80, 0xBF }, { 0x80, 0xBF }, 
	{ 0xA0, 0xBF }, { 0x80, 0xBF }, { 0x80, 0xBF }, { 0x80, 0xBF }, 
	{ 0x80, 0xBF }, { 0x80, 0xBF }, { 0x80, 0xBF }, { 0x80, 0xBF }, 
	{ 0x80, 0xBF }, { 0x80, 0xBF }, { 0x80, 0xBF }, { 0x80, 0xBF }, 
	{ 0x80, 0xBF }, { 0x80, 0x9F }, { 0x80, 0xBF }, { 0x80, 0xBF }, 
	{ 0x90, 0xBF }, { 0x80, 0xBF }, { 0x80, 0xBF }, { 0x80, 0xBF }, 
	{ 0x80, 0x8F }, { 0x00, 0x00 }, { 0x00, 0x00 }, { 0x00, 0x00 }, 
	{ 0x00, 0x00 }, { 0x00, 0x00 }, { 0x00, 0x00 }, { 0x00, 0x00 }, 
	{ 0x00, 0x00 }, { 0x00, 0x00 }, { 0x00, 0x00 }, { 0x00, 0x00 }, 
};

const byte UTF8::mrk[] = {0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC};

const ushort UTF8::upperrng[] =
{
	0x0041,	0x005a, 532,	/* A-Z a-z */
	0x00c0,	0x00d6, 532,	/* À-Ö à-ö */
	0x00d8,	0x00de, 532,	/* Ø-Þ ø-þ */
	0x0189,	0x018a, 705,	/* Ð-? ?-? */
	0x018e,	0x018f, 702,	/* ?-? ?-? */
	0x01b1,	0x01b2, 717,	/* ?-? ?-? */
	0x0388,	0x038a, 537,	/* ?-? ?-? */
	0x038e,	0x038f, 563,	/* ?-? ?-? */
	0x0391,	0x03a1, 532,	/* ?-? a-? */
	0x03a3,	0x03ab, 532,	/* S-? s-? */
	0x0401,	0x040c, 580,	/* ?-? ?-? */
	0x040e,	0x040f, 580,	/* ?-? ?-? */
	0x0410,	0x042f, 532,	/* ?-? ?-? */
	0x0531,	0x0556, 548,	/* ?-? ?-? */
	0x10a0,	0x10c5, 548,	/* ?-? ?-? */
	0x1f08,	0x1f0f, 492,	/* ?-? ?-? */
	0x1f18,	0x1f1d, 492,	/* ?-? ?-? */
	0x1f28,	0x1f2f, 492,	/* ?-? ?-? */
	0x1f38,	0x1f3f, 492,	/* ?-? ?-? */
	0x1f48,	0x1f4d, 492,	/* ?-? ?-? */
	0x1f68,	0x1f6f, 492,	/* ?-? ?-? */
	0x1f88,	0x1f8f, 492,	/* ?-? ?-? */
	0x1f98,	0x1f9f, 492,	/* ?-? ?-? */
	0x1fa8,	0x1faf, 492,	/* ?-? ?-? */
	0x1fb8,	0x1fb9, 492,	/* ?-? ?-? */
	0x1fba,	0x1fbb, 426,	/* ?-? ?-? */
	0x1fc8,	0x1fcb, 414,	/* ?-? ?-? */
	0x1fd8,	0x1fd9, 492,	/* ?-? ?-? */
	0x1fda,	0x1fdb, 400,	/* ?-? ?-? */
	0x1fe8,	0x1fe9, 492,	/* ?-? ?-? */
	0x1fea,	0x1feb, 388,	/* ?-? ?-? */
	0x1ff8,	0x1ff9, 372,	/* ?-? ?-? */
	0x1ffa,	0x1ffb, 374,	/* ?-? ?-? */
	0x2160,	0x216f, 516,	/* ?-? ?-? */
	0x24b6,	0x24cf, 526,	/* ?-? ?-? */
	0xff21,	0xff3a, 532,	/* A-Z a-z */
};

const ulong UTF8::nupperrng = sizeof(upperrng)/(sizeof(upperrng[0]*3));

const ushort UTF8::uppersgl[] =
{
	0x0100, 501,	/* A a */
	0x0102, 501,	/* A a */
	0x0104, 501,	/* A a */
	0x0106, 501,	/* C c */
	0x0108, 501,	/* C c */
	0x010a, 501,	/* C c */
	0x010c, 501,	/* C c */
	0x010e, 501,	/* D d */
	0x0110, 501,	/* Ð d */
	0x0112, 501,	/* E e */
	0x0114, 501,	/* E e */
	0x0116, 501,	/* E e */
	0x0118, 501,	/* E e */
	0x011a, 501,	/* E e */
	0x011c, 501,	/* G g */
	0x011e, 501,	/* G g */
	0x0120, 501,	/* G g */
	0x0122, 501,	/* G g */
	0x0124, 501,	/* H h */
	0x0126, 501,	/* H h */
	0x0128, 501,	/* I i */
	0x012a, 501,	/* I i */
	0x012c, 501,	/* I i */
	0x012e, 501,	/* I i */
	0x0130, 301,	/* I i */
	0x0132, 501,	/* ? ? */
	0x0134, 501,	/* J j */
	0x0136, 501,	/* K k */
	0x0139, 501,	/* L l */
	0x013b, 501,	/* L l */
	0x013d, 501,	/* L l */
	0x013f, 501,	/* ? ? */
	0x0141, 501,	/* L l */
	0x0143, 501,	/* N n */
	0x0145, 501,	/* N n */
	0x0147, 501,	/* N n */
	0x014a, 501,	/* ? ? */
	0x014c, 501,	/* O o */
	0x014e, 501,	/* O o */
	0x0150, 501,	/* O o */
	0x0152, 501,	/*   */
	0x0154, 501,	/* R r */
	0x0156, 501,	/* R r */
	0x0158, 501,	/* R r */
	0x015a, 501,	/* S s */
	0x015c, 501,	/* S s */
	0x015e, 501,	/* S s */
	0x0160, 501,	/*   */
	0x0162, 501,	/* T t */
	0x0164, 501,	/* T t */
	0x0166, 501,	/* T t */
	0x0168, 501,	/* U u */
	0x016a, 501,	/* U u */
	0x016c, 501,	/* U u */
	0x016e, 501,	/* U u */
	0x0170, 501,	/* U u */
	0x0172, 501,	/* U u */
	0x0174, 501,	/* W w */
	0x0176, 501,	/* Y y */
	0x0178, 379,	/*  ÿ */
	0x0179, 501,	/* Z z */
	0x017b, 501,	/* Z z */
	0x017d, 501,	/*   */
	0x0181, 710,	/* ? ? */
	0x0182, 501,	/* ? ? */
	0x0184, 501,	/* ? ? */
	0x0186, 706,	/* ? ? */
	0x0187, 501,	/* ? ? */
	0x018b, 501,	/* ? ? */
	0x0190, 703,	/* ? ? */
	0x0191, 501,	/*   */
	0x0193, 705,	/* ? ? */
	0x0194, 707,	/* ? ? */
	0x0196, 711,	/* ? ? */
	0x0197, 709,	/* I ? */
	0x0198, 501,	/* ? ? */
	0x019c, 711,	/* ? ? */
	0x019d, 713,	/* ? ? */
	0x01a0, 501,	/* O o */
	0x01a2, 501,	/* ? ? */
	0x01a4, 501,	/* ? ? */
	0x01a7, 501,	/* ? ? */
	0x01a9, 718,	/* ? ? */
	0x01ac, 501,	/* ? ? */
	0x01ae, 718,	/* T ? */
	0x01af, 501,	/* U u */
	0x01b3, 501,	/* ? ? */
	0x01b5, 501,	/* ? z */
	0x01b7, 719,	/* ? ? */
	0x01b8, 501,	/* ? ? */
	0x01bc, 501,	/* ? ? */
	0x01c4, 502,	/* ? ? */
	0x01c5, 501,	/* ? ? */
	0x01c7, 502,	/* ? ? */
	0x01c8, 501,	/* ? ? */
	0x01ca, 502,	/* ? ? */
	0x01cb, 501,	/* ? ? */
	0x01cd, 501,	/* A a */
	0x01cf, 501,	/* I i */
	0x01d1, 501,	/* O o */
	0x01d3, 501,	/* U u */
	0x01d5, 501,	/* U u */
	0x01d7, 501,	/* U u */
	0x01d9, 501,	/* U u */
	0x01db, 501,	/* U u */
	0x01de, 501,	/* A a */
	0x01e0, 501,	/* ? ? */
	0x01e2, 501,	/* ? ? */
	0x01e4, 501,	/* G g */
	0x01e6, 501,	/* G g */
	0x01e8, 501,	/* K k */
	0x01ea, 501,	/* O o */
	0x01ec, 501,	/* O o */
	0x01ee, 501,	/* ? ? */
	0x01f1, 502,	/* ? ? */
	0x01f2, 501,	/* ? ? */
	0x01f4, 501,	/* ? ? */
	0x01fa, 501,	/* ? ? */
	0x01fc, 501,	/* ? ? */
	0x01fe, 501,	/* ? ? */
	0x0200, 501,	/* ? ? */
	0x0202, 501,	/* ? ? */
	0x0204, 501,	/* ? ? */
	0x0206, 501,	/* ? ? */
	0x0208, 501,	/* ? ? */
	0x020a, 501,	/* ? ? */
	0x020c, 501,	/* ? ? */
	0x020e, 501,	/* ? ? */
	0x0210, 501,	/* ? ? */
	0x0212, 501,	/* ? ? */
	0x0214, 501,	/* ? ? */
	0x0216, 501,	/* ? ? */
	0x0386, 538,	/* ? ? */
	0x038c, 564,	/* ? ? */
	0x03e2, 501,	/* ? ? */
	0x03e4, 501,	/* ? ? */
	0x03e6, 501,	/* ? ? */
	0x03e8, 501,	/* ? ? */
	0x03ea, 501,	/* ? ? */
	0x03ec, 501,	/* ? ? */
	0x03ee, 501,	/* ? ? */
	0x0460, 501,	/* ? ? */
	0x0462, 501,	/* ? ? */
	0x0464, 501,	/* ? ? */
	0x0466, 501,	/* ? ? */
	0x0468, 501,	/* ? ? */
	0x046a, 501,	/* ? ? */
	0x046c, 501,	/* ? ? */
	0x046e, 501,	/* ? ? */
	0x0470, 501,	/* ? ? */
	0x0472, 501,	/* ? ? */
	0x0474, 501,	/* ? ? */
	0x0476, 501,	/* ? ? */
	0x0478, 501,	/* ? ? */
	0x047a, 501,	/* ? ? */
	0x047c, 501,	/* ? ? */
	0x047e, 501,	/* ? ? */
	0x0480, 501,	/* ? ? */
	0x0490, 501,	/* ? ? */
	0x0492, 501,	/* ? ? */
	0x0494, 501,	/* ? ? */
	0x0496, 501,	/* ? ? */
	0x0498, 501,	/* ? ? */
	0x049a, 501,	/* ? ? */
	0x049c, 501,	/* ? ? */
	0x049e, 501,	/* ? ? */
	0x04a0, 501,	/* ? ? */
	0x04a2, 501,	/* ? ? */
	0x04a4, 501,	/* ? ? */
	0x04a6, 501,	/* ? ? */
	0x04a8, 501,	/* ? ? */
	0x04aa, 501,	/* ? ? */
	0x04ac, 501,	/* ? ? */
	0x04ae, 501,	/* ? ? */
	0x04b0, 501,	/* ? ? */
	0x04b2, 501,	/* ? ? */
	0x04b4, 501,	/* ? ? */
	0x04b6, 501,	/* ? ? */
	0x04b8, 501,	/* ? ? */
	0x04ba, 501,	/* ? h */
	0x04bc, 501,	/* ? ? */
	0x04be, 501,	/* ? ? */
	0x04c1, 501,	/* ? ? */
	0x04c3, 501,	/* ? ? */
	0x04c7, 501,	/* ? ? */
	0x04cb, 501,	/* ? ? */
	0x04d0, 501,	/* ? ? */
	0x04d2, 501,	/* ? ? */
	0x04d4, 501,	/* ? ? */
	0x04d6, 501,	/* ? ? */
	0x04d8, 501,	/* ? ? */
	0x04da, 501,	/* ? ? */
	0x04dc, 501,	/* ? ? */
	0x04de, 501,	/* ? ? */
	0x04e0, 501,	/* ? ? */
	0x04e2, 501,	/* ? ? */
	0x04e4, 501,	/* ? ? */
	0x04e6, 501,	/* ? ? */
	0x04e8, 501,	/* ? ? */
	0x04ea, 501,	/* ? ? */
	0x04ee, 501,	/* ? ? */
	0x04f0, 501,	/* ? ? */
	0x04f2, 501,	/* ? ? */
	0x04f4, 501,	/* ? ? */
	0x04f8, 501,	/* ? ? */
	0x1e00, 501,	/* ? ? */
	0x1e02, 501,	/* ? ? */
	0x1e04, 501,	/* ? ? */
	0x1e06, 501,	/* ? ? */
	0x1e08, 501,	/* ? ? */
	0x1e0a, 501,	/* ? ? */
	0x1e0c, 501,	/* ? ? */
	0x1e0e, 501,	/* ? ? */
	0x1e10, 501,	/* ? ? */
	0x1e12, 501,	/* ? ? */
	0x1e14, 501,	/* ? ? */
	0x1e16, 501,	/* ? ? */
	0x1e18, 501,	/* ? ? */
	0x1e1a, 501,	/* ? ? */
	0x1e1c, 501,	/* ? ? */
	0x1e1e, 501,	/* ? ? */
	0x1e20, 501,	/* ? ? */
	0x1e22, 501,	/* ? ? */
	0x1e24, 501,	/* ? ? */
	0x1e26, 501,	/* ? ? */
	0x1e28, 501,	/* ? ? */
	0x1e2a, 501,	/* ? ? */
	0x1e2c, 501,	/* ? ? */
	0x1e2e, 501,	/* ? ? */
	0x1e30, 501,	/* ? ? */
	0x1e32, 501,	/* ? ? */
	0x1e34, 501,	/* ? ? */
	0x1e36, 501,	/* ? ? */
	0x1e38, 501,	/* ? ? */
	0x1e3a, 501,	/* ? ? */
	0x1e3c, 501,	/* ? ? */
	0x1e3e, 501,	/* ? ? */
	0x1e40, 501,	/* ? ? */
	0x1e42, 501,	/* ? ? */
	0x1e44, 501,	/* ? ? */
	0x1e46, 501,	/* ? ? */
	0x1e48, 501,	/* ? ? */
	0x1e4a, 501,	/* ? ? */
	0x1e4c, 501,	/* ? ? */
	0x1e4e, 501,	/* ? ? */
	0x1e50, 501,	/* ? ? */
	0x1e52, 501,	/* ? ? */
	0x1e54, 501,	/* ? ? */
	0x1e56, 501,	/* ? ? */
	0x1e58, 501,	/* ? ? */
	0x1e5a, 501,	/* ? ? */
	0x1e5c, 501,	/* ? ? */
	0x1e5e, 501,	/* ? ? */
	0x1e60, 501,	/* ? ? */
	0x1e62, 501,	/* ? ? */
	0x1e64, 501,	/* ? ? */
	0x1e66, 501,	/* ? ? */
	0x1e68, 501,	/* ? ? */
	0x1e6a, 501,	/* ? ? */
	0x1e6c, 501,	/* ? ? */
	0x1e6e, 501,	/* ? ? */
	0x1e70, 501,	/* ? ? */
	0x1e72, 501,	/* ? ? */
	0x1e74, 501,	/* ? ? */
	0x1e76, 501,	/* ? ? */
	0x1e78, 501,	/* ? ? */
	0x1e7a, 501,	/* ? ? */
	0x1e7c, 501,	/* ? ? */
	0x1e7e, 501,	/* ? ? */
	0x1e80, 501,	/* ? ? */
	0x1e82, 501,	/* ? ? */
	0x1e84, 501,	/* ? ? */
	0x1e86, 501,	/* ? ? */
	0x1e88, 501,	/* ? ? */
	0x1e8a, 501,	/* ? ? */
	0x1e8c, 501,	/* ? ? */
	0x1e8e, 501,	/* ? ? */
	0x1e90, 501,	/* ? ? */
	0x1e92, 501,	/* ? ? */
	0x1e94, 501,	/* ? ? */
	0x1ea0, 501,	/* ? ? */
	0x1ea2, 501,	/* ? ? */
	0x1ea4, 501,	/* ? ? */
	0x1ea6, 501,	/* ? ? */
	0x1ea8, 501,	/* ? ? */
	0x1eaa, 501,	/* ? ? */
	0x1eac, 501,	/* ? ? */
	0x1eae, 501,	/* ? ? */
	0x1eb0, 501,	/* ? ? */
	0x1eb2, 501,	/* ? ? */
	0x1eb4, 501,	/* ? ? */
	0x1eb6, 501,	/* ? ? */
	0x1eb8, 501,	/* ? ? */
	0x1eba, 501,	/* ? ? */
	0x1ebc, 501,	/* ? ? */
	0x1ebe, 501,	/* ? ? */
	0x1ec0, 501,	/* ? ? */
	0x1ec2, 501,	/* ? ? */
	0x1ec4, 501,	/* ? ? */
	0x1ec6, 501,	/* ? ? */
	0x1ec8, 501,	/* ? ? */
	0x1eca, 501,	/* ? ? */
	0x1ecc, 501,	/* ? ? */
	0x1ece, 501,	/* ? ? */
	0x1ed0, 501,	/* ? ? */
	0x1ed2, 501,	/* ? ? */
	0x1ed4, 501,	/* ? ? */
	0x1ed6, 501,	/* ? ? */
	0x1ed8, 501,	/* ? ? */
	0x1eda, 501,	/* ? ? */
	0x1edc, 501,	/* ? ? */
	0x1ede, 501,	/* ? ? */
	0x1ee0, 501,	/* ? ? */
	0x1ee2, 501,	/* ? ? */
	0x1ee4, 501,	/* ? ? */
	0x1ee6, 501,	/* ? ? */
	0x1ee8, 501,	/* ? ? */
	0x1eea, 501,	/* ? ? */
	0x1eec, 501,	/* ? ? */
	0x1eee, 501,	/* ? ? */
	0x1ef0, 501,	/* ? ? */
	0x1ef2, 501,	/* ? ? */
	0x1ef4, 501,	/* ? ? */
	0x1ef6, 501,	/* ? ? */
	0x1ef8, 501,	/* ? ? */
	0x1f59, 492,	/* ? ? */
	0x1f5b, 492,	/* ? ? */
	0x1f5d, 492,	/* ? ? */
	0x1f5f, 492,	/* ? ? */
	0x1fbc, 491,	/* ? ? */
	0x1fcc, 491,	/* ? ? */
	0x1fec, 493,	/* ? ? */
	0x1ffc, 491,	/* ? ? */
};

const ulong UTF8::nuppersgl = sizeof(uppersgl)/(sizeof(uppersgl[0]*2));

const ushort UTF8::lowerrng[] =
{
	0x0061,	0x007a, 468,	/* a-z A-Z */
	0x00e0,	0x00f6, 468,	/* à-ö À-Ö */
	0x00f8,	0x00fe, 468,	/* ø-þ Ø-Þ */
	0x0256,	0x0257, 295,	/* ?-? Ð-? */
	0x0258,	0x0259, 298,	/* ?-? ?-? */
	0x028a,	0x028b, 283,	/* ?-? ?-? */
	0x03ad,	0x03af, 463,	/* ?-? ?-? */
	0x03b1,	0x03c1, 468,	/* a-? ?-? */
	0x03c3,	0x03cb, 468,	/* s-? S-? */
	0x03cd,	0x03ce, 437,	/* ?-? ?-? */
	0x0430,	0x044f, 468,	/* ?-? ?-? */
	0x0451,	0x045c, 420,	/* ?-? ?-? */
	0x045e,	0x045f, 420,	/* ?-? ?-? */
	0x0561,	0x0586, 452,	/* ?-? ?-? */
	0x1f00,	0x1f07, 508,	/* ?-? ?-? */
	0x1f10,	0x1f15, 508,	/* ?-? ?-? */
	0x1f20,	0x1f27, 508,	/* ?-? ?-? */
	0x1f30,	0x1f37, 508,	/* ?-? ?-? */
	0x1f40,	0x1f45, 508,	/* ?-? ?-? */
	0x1f60,	0x1f67, 508,	/* ?-? ?-? */
	0x1f70,	0x1f71, 574,	/* ?-? ?-? */
	0x1f72,	0x1f75, 586,	/* ?-? ?-? */
	0x1f76,	0x1f77, 600,	/* ?-? ?-? */
	0x1f78,	0x1f79, 628,	/* ?-? ?-? */
	0x1f7a,	0x1f7b, 612,	/* ?-? ?-? */
	0x1f7c,	0x1f7d, 626,	/* ?-? ?-? */
	0x1f80,	0x1f87, 508,	/* ?-? ?-? */
	0x1f90,	0x1f97, 508,	/* ?-? ?-? */
	0x1fa0,	0x1fa7, 508,	/* ?-? ?-? */
	0x1fb0,	0x1fb1, 508,	/* ?-? ?-? */
	0x1fd0,	0x1fd1, 508,	/* ?-? ?-? */
	0x1fe0,	0x1fe1, 508,	/* ?-? ?-? */
	0x2170,	0x217f, 484,	/* ?-? ?-? */
	0x24d0,	0x24e9, 474,	/* ?-? ?-? */
	0xff41,	0xff5a, 468,	/* a-z A-Z */
};

const ulong UTF8::nlowerrng = sizeof(lowerrng)/(sizeof(lowerrng[0]*3));

const ushort UTF8::lowersgl[] =
{
	0x00ff, 621,	/* ÿ  */
	0x0101, 499,	/* a A */
	0x0103, 499,	/* a A */
	0x0105, 499,	/* a A */
	0x0107, 499,	/* c C */
	0x0109, 499,	/* c C */
	0x010b, 499,	/* c C */
	0x010d, 499,	/* c C */
	0x010f, 499,	/* d D */
	0x0111, 499,	/* d Ð */
	0x0113, 499,	/* e E */
	0x0115, 499,	/* e E */
	0x0117, 499,	/* e E */
	0x0119, 499,	/* e E */
	0x011b, 499,	/* e E */
	0x011d, 499,	/* g G */
	0x011f, 499,	/* g G */
	0x0121, 499,	/* g G */
	0x0123, 499,	/* g G */
	0x0125, 499,	/* h H */
	0x0127, 499,	/* h H */
	0x0129, 499,	/* i I */
	0x012b, 499,	/* i I */
	0x012d, 499,	/* i I */
	0x012f, 499,	/* i I */
	0x0131, 268,	/* i I */
	0x0133, 499,	/* ? ? */
	0x0135, 499,	/* j J */
	0x0137, 499,	/* k K */
	0x013a, 499,	/* l L */
	0x013c, 499,	/* l L */
	0x013e, 499,	/* l L */
	0x0140, 499,	/* ? ? */
	0x0142, 499,	/* l L */
	0x0144, 499,	/* n N */
	0x0146, 499,	/* n N */
	0x0148, 499,	/* n N */
	0x014b, 499,	/* ? ? */
	0x014d, 499,	/* o O */
	0x014f, 499,	/* o O */
	0x0151, 499,	/* o O */
	0x0153, 499,	/*   */
	0x0155, 499,	/* r R */
	0x0157, 499,	/* r R */
	0x0159, 499,	/* r R */
	0x015b, 499,	/* s S */
	0x015d, 499,	/* s S */
	0x015f, 499,	/* s S */
	0x0161, 499,	/*   */
	0x0163, 499,	/* t T */
	0x0165, 499,	/* t T */
	0x0167, 499,	/* t T */
	0x0169, 499,	/* u U */
	0x016b, 499,	/* u U */
	0x016d, 499,	/* u U */
	0x016f, 499,	/* u U */
	0x0171, 499,	/* u U */
	0x0173, 499,	/* u U */
	0x0175, 499,	/* w W */
	0x0177, 499,	/* y Y */
	0x017a, 499,	/* z Z */
	0x017c, 499,	/* z Z */
	0x017e, 499,	/*   */
	0x017f, 200,	/* ? S */
	0x0183, 499,	/* ? ? */
	0x0185, 499,	/* ? ? */
	0x0188, 499,	/* ? ? */
	0x018c, 499,	/* ? ? */
	0x0192, 499,	/*   */
	0x0199, 499,	/* ? ? */
	0x01a1, 499,	/* o O */
	0x01a3, 499,	/* ? ? */
	0x01a5, 499,	/* ? ? */
	0x01a8, 499,	/* ? ? */
	0x01ad, 499,	/* ? ? */
	0x01b0, 499,	/* u U */
	0x01b4, 499,	/* ? ? */
	0x01b6, 499,	/* z ? */
	0x01b9, 499,	/* ? ? */
	0x01bd, 499,	/* ? ? */
	0x01c5, 499,	/* ? ? */
	0x01c6, 498,	/* ? ? */
	0x01c8, 499,	/* ? ? */
	0x01c9, 498,	/* ? ? */
	0x01cb, 499,	/* ? ? */
	0x01cc, 498,	/* ? ? */
	0x01ce, 499,	/* a A */
	0x01d0, 499,	/* i I */
	0x01d2, 499,	/* o O */
	0x01d4, 499,	/* u U */
	0x01d6, 499,	/* u U */
	0x01d8, 499,	/* u U */
	0x01da, 499,	/* u U */
	0x01dc, 499,	/* u U */
	0x01df, 499,	/* a A */
	0x01e1, 499,	/* ? ? */
	0x01e3, 499,	/* ? ? */
	0x01e5, 499,	/* g G */
	0x01e7, 499,	/* g G */
	0x01e9, 499,	/* k K */
	0x01eb, 499,	/* o O */
	0x01ed, 499,	/* o O */
	0x01ef, 499,	/* ? ? */
	0x01f2, 499,	/* ? ? */
	0x01f3, 498,	/* ? ? */
	0x01f5, 499,	/* ? ? */
	0x01fb, 499,	/* ? ? */
	0x01fd, 499,	/* ? ? */
	0x01ff, 499,	/* ? ? */
	0x0201, 499,	/* ? ? */
	0x0203, 499,	/* ? ? */
	0x0205, 499,	/* ? ? */
	0x0207, 499,	/* ? ? */
	0x0209, 499,	/* ? ? */
	0x020b, 499,	/* ? ? */
	0x020d, 499,	/* ? ? */
	0x020f, 499,	/* ? ? */
	0x0211, 499,	/* ? ? */
	0x0213, 499,	/* ? ? */
	0x0215, 499,	/* ? ? */
	0x0217, 499,	/* ? ? */
	0x0253, 290,	/* ? ? */
	0x0254, 294,	/* ? ? */
	0x025b, 297,	/* ? ? */
	0x0260, 295,	/* ? ? */
	0x0263, 293,	/* ? ? */
	0x0268, 291,	/* ? I */
	0x0269, 289,	/* ? ? */
	0x026f, 289,	/* ? ? */
	0x0272, 287,	/* ? ? */
	0x0283, 282,	/* ? ? */
	0x0288, 282,	/* ? T */
	0x0292, 281,	/* ? ? */
	0x03ac, 462,	/* ? ? */
	0x03cc, 436,	/* ? ? */
	0x03d0, 438,	/* ? ? */
	0x03d1, 443,	/* ? T */
	0x03d5, 453,	/* ? F */
	0x03d6, 446,	/* ? ? */
	0x03e3, 499,	/* ? ? */
	0x03e5, 499,	/* ? ? */
	0x03e7, 499,	/* ? ? */
	0x03e9, 499,	/* ? ? */
	0x03eb, 499,	/* ? ? */
	0x03ed, 499,	/* ? ? */
	0x03ef, 499,	/* ? ? */
	0x03f0, 414,	/* ? ? */
	0x03f1, 420,	/* ? ? */
	0x0461, 499,	/* ? ? */
	0x0463, 499,	/* ? ? */
	0x0465, 499,	/* ? ? */
	0x0467, 499,	/* ? ? */
	0x0469, 499,	/* ? ? */
	0x046b, 499,	/* ? ? */
	0x046d, 499,	/* ? ? */
	0x046f, 499,	/* ? ? */
	0x0471, 499,	/* ? ? */
	0x0473, 499,	/* ? ? */
	0x0475, 499,	/* ? ? */
	0x0477, 499,	/* ? ? */
	0x0479, 499,	/* ? ? */
	0x047b, 499,	/* ? ? */
	0x047d, 499,	/* ? ? */
	0x047f, 499,	/* ? ? */
	0x0481, 499,	/* ? ? */
	0x0491, 499,	/* ? ? */
	0x0493, 499,	/* ? ? */
	0x0495, 499,	/* ? ? */
	0x0497, 499,	/* ? ? */
	0x0499, 499,	/* ? ? */
	0x049b, 499,	/* ? ? */
	0x049d, 499,	/* ? ? */
	0x049f, 499,	/* ? ? */
	0x04a1, 499,	/* ? ? */
	0x04a3, 499,	/* ? ? */
	0x04a5, 499,	/* ? ? */
	0x04a7, 499,	/* ? ? */
	0x04a9, 499,	/* ? ? */
	0x04ab, 499,	/* ? ? */
	0x04ad, 499,	/* ? ? */
	0x04af, 499,	/* ? ? */
	0x04b1, 499,	/* ? ? */
	0x04b3, 499,	/* ? ? */
	0x04b5, 499,	/* ? ? */
	0x04b7, 499,	/* ? ? */
	0x04b9, 499,	/* ? ? */
	0x04bb, 499,	/* h ? */
	0x04bd, 499,	/* ? ? */
	0x04bf, 499,	/* ? ? */
	0x04c2, 499,	/* ? ? */
	0x04c4, 499,	/* ? ? */
	0x04c8, 499,	/* ? ? */
	0x04cc, 499,	/* ? ? */
	0x04d1, 499,	/* ? ? */
	0x04d3, 499,	/* ? ? */
	0x04d5, 499,	/* ? ? */
	0x04d7, 499,	/* ? ? */
	0x04d9, 499,	/* ? ? */
	0x04db, 499,	/* ? ? */
	0x04dd, 499,	/* ? ? */
	0x04df, 499,	/* ? ? */
	0x04e1, 499,	/* ? ? */
	0x04e3, 499,	/* ? ? */
	0x04e5, 499,	/* ? ? */
	0x04e7, 499,	/* ? ? */
	0x04e9, 499,	/* ? ? */
	0x04eb, 499,	/* ? ? */
	0x04ef, 499,	/* ? ? */
	0x04f1, 499,	/* ? ? */
	0x04f3, 499,	/* ? ? */
	0x04f5, 499,	/* ? ? */
	0x04f9, 499,	/* ? ? */
	0x1e01, 499,	/* ? ? */
	0x1e03, 499,	/* ? ? */
	0x1e05, 499,	/* ? ? */
	0x1e07, 499,	/* ? ? */
	0x1e09, 499,	/* ? ? */
	0x1e0b, 499,	/* ? ? */
	0x1e0d, 499,	/* ? ? */
	0x1e0f, 499,	/* ? ? */
	0x1e11, 499,	/* ? ? */
	0x1e13, 499,	/* ? ? */
	0x1e15, 499,	/* ? ? */
	0x1e17, 499,	/* ? ? */
	0x1e19, 499,	/* ? ? */
	0x1e1b, 499,	/* ? ? */
	0x1e1d, 499,	/* ? ? */
	0x1e1f, 499,	/* ? ? */
	0x1e21, 499,	/* ? ? */
	0x1e23, 499,	/* ? ? */
	0x1e25, 499,	/* ? ? */
	0x1e27, 499,	/* ? ? */
	0x1e29, 499,	/* ? ? */
	0x1e2b, 499,	/* ? ? */
	0x1e2d, 499,	/* ? ? */
	0x1e2f, 499,	/* ? ? */
	0x1e31, 499,	/* ? ? */
	0x1e33, 499,	/* ? ? */
	0x1e35, 499,	/* ? ? */
	0x1e37, 499,	/* ? ? */
	0x1e39, 499,	/* ? ? */
	0x1e3b, 499,	/* ? ? */
	0x1e3d, 499,	/* ? ? */
	0x1e3f, 499,	/* ? ? */
	0x1e41, 499,	/* ? ? */
	0x1e43, 499,	/* ? ? */
	0x1e45, 499,	/* ? ? */
	0x1e47, 499,	/* ? ? */
	0x1e49, 499,	/* ? ? */
	0x1e4b, 499,	/* ? ? */
	0x1e4d, 499,	/* ? ? */
	0x1e4f, 499,	/* ? ? */
	0x1e51, 499,	/* ? ? */
	0x1e53, 499,	/* ? ? */
	0x1e55, 499,	/* ? ? */
	0x1e57, 499,	/* ? ? */
	0x1e59, 499,	/* ? ? */
	0x1e5b, 499,	/* ? ? */
	0x1e5d, 499,	/* ? ? */
	0x1e5f, 499,	/* ? ? */
	0x1e61, 499,	/* ? ? */
	0x1e63, 499,	/* ? ? */
	0x1e65, 499,	/* ? ? */
	0x1e67, 499,	/* ? ? */
	0x1e69, 499,	/* ? ? */
	0x1e6b, 499,	/* ? ? */
	0x1e6d, 499,	/* ? ? */
	0x1e6f, 499,	/* ? ? */
	0x1e71, 499,	/* ? ? */
	0x1e73, 499,	/* ? ? */
	0x1e75, 499,	/* ? ? */
	0x1e77, 499,	/* ? ? */
	0x1e79, 499,	/* ? ? */
	0x1e7b, 499,	/* ? ? */
	0x1e7d, 499,	/* ? ? */
	0x1e7f, 499,	/* ? ? */
	0x1e81, 499,	/* ? ? */
	0x1e83, 499,	/* ? ? */
	0x1e85, 499,	/* ? ? */
	0x1e87, 499,	/* ? ? */
	0x1e89, 499,	/* ? ? */
	0x1e8b, 499,	/* ? ? */
	0x1e8d, 499,	/* ? ? */
	0x1e8f, 499,	/* ? ? */
	0x1e91, 499,	/* ? ? */
	0x1e93, 499,	/* ? ? */
	0x1e95, 499,	/* ? ? */
	0x1ea1, 499,	/* ? ? */
	0x1ea3, 499,	/* ? ? */
	0x1ea5, 499,	/* ? ? */
	0x1ea7, 499,	/* ? ? */
	0x1ea9, 499,	/* ? ? */
	0x1eab, 499,	/* ? ? */
	0x1ead, 499,	/* ? ? */
	0x1eaf, 499,	/* ? ? */
	0x1eb1, 499,	/* ? ? */
	0x1eb3, 499,	/* ? ? */
	0x1eb5, 499,	/* ? ? */
	0x1eb7, 499,	/* ? ? */
	0x1eb9, 499,	/* ? ? */
	0x1ebb, 499,	/* ? ? */
	0x1ebd, 499,	/* ? ? */
	0x1ebf, 499,	/* ? ? */
	0x1ec1, 499,	/* ? ? */
	0x1ec3, 499,	/* ? ? */
	0x1ec5, 499,	/* ? ? */
	0x1ec7, 499,	/* ? ? */
	0x1ec9, 499,	/* ? ? */
	0x1ecb, 499,	/* ? ? */
	0x1ecd, 499,	/* ? ? */
	0x1ecf, 499,	/* ? ? */
	0x1ed1, 499,	/* ? ? */
	0x1ed3, 499,	/* ? ? */
	0x1ed5, 499,	/* ? ? */
	0x1ed7, 499,	/* ? ? */
	0x1ed9, 499,	/* ? ? */
	0x1edb, 499,	/* ? ? */
	0x1edd, 499,	/* ? ? */
	0x1edf, 499,	/* ? ? */
	0x1ee1, 499,	/* ? ? */
	0x1ee3, 499,	/* ? ? */
	0x1ee5, 499,	/* ? ? */
	0x1ee7, 499,	/* ? ? */
	0x1ee9, 499,	/* ? ? */
	0x1eeb, 499,	/* ? ? */
	0x1eed, 499,	/* ? ? */
	0x1eef, 499,	/* ? ? */
	0x1ef1, 499,	/* ? ? */
	0x1ef3, 499,	/* ? ? */
	0x1ef5, 499,	/* ? ? */
	0x1ef7, 499,	/* ? ? */
	0x1ef9, 499,	/* ? ? */
	0x1f51, 508,	/* ? ? */
	0x1f53, 508,	/* ? ? */
	0x1f55, 508,	/* ? ? */
	0x1f57, 508,	/* ? ? */
	0x1fb3, 509,	/* ? ? */
	0x1fc3, 509,	/* ? ? */
	0x1fe5, 507,	/* ? ? */
	0x1ff3, 509,	/* ? ? */
};

const ulong UTF8::nlowersgl = sizeof(lowersgl)/(sizeof(lowersgl[0]*2));

const ushort UTF8::otherrng[] = 
{
	0x00d8,	0x00f6,	/* Ø - ö */
	0x00f8,	0x01f5,	/* ø - ? */
	0x0250,	0x02a8,	/* ? - ? */
	0x038e,	0x03a1,	/* ? - ? */
	0x03a3,	0x03ce,	/* S - ? */
	0x03d0,	0x03d6,	/* ? - ? */
	0x03e2,	0x03f3,	/* ? - ? */
	0x0490,	0x04c4,	/* ? - ? */
	0x0561,	0x0587,	/* ? - ? */
	0x05d0,	0x05ea,	/* ? - ? */
	0x05f0,	0x05f2,	/* ? - ? */
	0x0621,	0x063a,	/* ? - ? */
	0x0640,	0x064a,	/* ? - ? */
	0x0671,	0x06b7,	/* ? - ? */
	0x06ba,	0x06be,	/* ? - ? */
	0x06c0,	0x06ce,	/* ? - ? */
	0x06d0,	0x06d3,	/* ? - ? */
	0x0905,	0x0939,	/* ? - ? */
	0x0958,	0x0961,	/* ? - ? */
	0x0985,	0x098c,	/* ? - ? */
	0x098f,	0x0990,	/* ? - ? */
	0x0993,	0x09a8,	/* ? - ? */
	0x09aa,	0x09b0,	/* ? - ? */
	0x09b6,	0x09b9,	/* ? - ? */
	0x09dc,	0x09dd,	/* ? - ? */
	0x09df,	0x09e1,	/* ? - ? */
	0x09f0,	0x09f1,	/* ? - ? */
	0x0a05,	0x0a0a,	/* ? - ? */
	0x0a0f,	0x0a10,	/* ? - ? */
	0x0a13,	0x0a28,	/* ? - ? */
	0x0a2a,	0x0a30,	/* ? - ? */
	0x0a32,	0x0a33,	/* ? - ? */
	0x0a35,	0x0a36,	/* ? - ? */
	0x0a38,	0x0a39,	/* ? - ? */
	0x0a59,	0x0a5c,	/* ? - ? */
	0x0a85,	0x0a8b,	/* ? - ? */
	0x0a8f,	0x0a91,	/* ? - ? */
	0x0a93,	0x0aa8,	/* ? - ? */
	0x0aaa,	0x0ab0,	/* ? - ? */
	0x0ab2,	0x0ab3,	/* ? - ? */
	0x0ab5,	0x0ab9,	/* ? - ? */
	0x0b05,	0x0b0c,	/* ? - ? */
	0x0b0f,	0x0b10,	/* ? - ? */
	0x0b13,	0x0b28,	/* ? - ? */
	0x0b2a,	0x0b30,	/* ? - ? */
	0x0b32,	0x0b33,	/* ? - ? */
	0x0b36,	0x0b39,	/* ? - ? */
	0x0b5c,	0x0b5d,	/* ? - ? */
	0x0b5f,	0x0b61,	/* ? - ? */
	0x0b85,	0x0b8a,	/* ? - ? */
	0x0b8e,	0x0b90,	/* ? - ? */
	0x0b92,	0x0b95,	/* ? - ? */
	0x0b99,	0x0b9a,	/* ? - ? */
	0x0b9e,	0x0b9f,	/* ? - ? */
	0x0ba3,	0x0ba4,	/* ? - ? */
	0x0ba8,	0x0baa,	/* ? - ? */
	0x0bae,	0x0bb5,	/* ? - ? */
	0x0bb7,	0x0bb9,	/* ? - ? */
	0x0c05,	0x0c0c,	/* ? - ? */
	0x0c0e,	0x0c10,	/* ? - ? */
	0x0c12,	0x0c28,	/* ? - ? */
	0x0c2a,	0x0c33,	/* ? - ? */
	0x0c35,	0x0c39,	/* ? - ? */
	0x0c60,	0x0c61,	/* ? - ? */
	0x0c85,	0x0c8c,	/* ? - ? */
	0x0c8e,	0x0c90,	/* ? - ? */
	0x0c92,	0x0ca8,	/* ? - ? */
	0x0caa,	0x0cb3,	/* ? - ? */
	0x0cb5,	0x0cb9,	/* ? - ? */
	0x0ce0,	0x0ce1,	/* ? - ? */
	0x0d05,	0x0d0c,	/* ? - ? */
	0x0d0e,	0x0d10,	/* ? - ? */
	0x0d12,	0x0d28,	/* ? - ? */
	0x0d2a,	0x0d39,	/* ? - ? */
	0x0d60,	0x0d61,	/* ? - ? */
	0x0e01,	0x0e30,	/* ? - ? */
	0x0e32,	0x0e33,	/* ? - ? */
	0x0e40,	0x0e46,	/* ? - ? */
	0x0e5a,	0x0e5b,	/* ? - ? */
	0x0e81,	0x0e82,	/* ? - ? */
	0x0e87,	0x0e88,	/* ? - ? */
	0x0e94,	0x0e97,	/* ? - ? */
	0x0e99,	0x0e9f,	/* ? - ? */
	0x0ea1,	0x0ea3,	/* ? - ? */
	0x0eaa,	0x0eab,	/* ? - ? */
	0x0ead,	0x0eae,	/* ? - ? */
	0x0eb2,	0x0eb3,	/* ? - ? */
	0x0ec0,	0x0ec4,	/* ? - ? */
	0x0edc,	0x0edd,	/* ? - ? */
	0x0f18,	0x0f19,	/* ? - ? */
	0x0f40,	0x0f47,	/* ? - ? */
	0x0f49,	0x0f69,	/* ? - ? */
	0x10d0,	0x10f6,	/* ? - ? */
	0x1100,	0x1159,	/* ? - ? */
	0x115f,	0x11a2,	/* ? - ? */
	0x11a8,	0x11f9,	/* ? - ? */
	0x1e00,	0x1e9b,	/* ? - ? */
	0x1f50,	0x1f57,	/* ? - ? */
	0x1f80,	0x1fb4,	/* ? - ? */
	0x1fb6,	0x1fbc,	/* ? - ? */
	0x1fc2,	0x1fc4,	/* ? - ? */
	0x1fc6,	0x1fcc,	/* ? - ? */
	0x1fd0,	0x1fd3,	/* ? - ? */
	0x1fd6,	0x1fdb,	/* ? - ? */
	0x1fe0,	0x1fec,	/* ? - ? */
	0x1ff2,	0x1ff4,	/* ? - ? */
	0x1ff6,	0x1ffc,	/* ? - ? */
	0x210a,	0x2113,	/* g - l */
	0x2115,	0x211d,	/* N - R */
	0x2120,	0x2122,	/* ? -  */
	0x212a,	0x2131,	/* K - F */
	0x2133,	0x2138,	/* M - ? */
	0x3041,	0x3094,	/* ? - ? */
	0x30a1,	0x30fa,	/* ? - ? */
	0x3105,	0x312c,	/* ? - ? */
	0x3131,	0x318e,	/* ? - ? */
	0x3192,	0x319f,	/* ? - ? */
	0x3260,	0x327b,	/* ? - ? */
	0x328a,	0x32b0,	/* ? - ? */
	0x32d0,	0x32fe,	/* ? - ? */
	0x3300,	0x3357,	/* ? - ? */
	0x3371,	0x3376,	/* ? - ? */
	0x337b,	0x3394,	/* ? - ? */
	0x3399,	0x339e,	/* ? - ? */
	0x33a9,	0x33ad,	/* ? - ? */
	0x33b0,	0x33c1,	/* ? - ? */
	0x33c3,	0x33c5,	/* ? - ? */
	0x33c7,	0x33d7,	/* ? - ? */
	0x33d9,	0x33dd,	/* ? - ? */
	0x4e00,	0x9fff,	/* ? - ? */
	0xac00,	0xd7a3,	/* ? - ? */
	0xf900,	0xfb06,	/* ? - ? */
	0xfb13,	0xfb17,	/* ? - ? */
	0xfb1f,	0xfb28,	/* ? - ? */
	0xfb2a,	0xfb36,	/* ? - ? */
	0xfb38,	0xfb3c,	/* ? - ? */
	0xfb40,	0xfb41,	/* ? - ? */
	0xfb43,	0xfb44,	/* ? - ? */
	0xfb46,	0xfbb1,	/* ? - ? */
	0xfbd3,	0xfd3d,	/* ? - ? */
	0xfd50,	0xfd8f,	/* ? - ? */
	0xfd92,	0xfdc7,	/* ? - ? */
	0xfdf0,	0xfdf9,	/* ? - ? */
	0xfe70,	0xfe72,	/* ? - ? */
	0xfe76,	0xfefc,	/* ? - ? */
	0xff66,	0xff6f,	/* ? - ? */
	0xff71,	0xff9d,	/* ? - ? */
	0xffa0,	0xffbe,	/* ? - ? */
	0xffc2,	0xffc7,	/* ? - ? */
	0xffca,	0xffcf,	/* ? - ? */
	0xffd2,	0xffd7,	/* ? - ? */
	0xffda,	0xffdc,	/* ? - ? */
};

const ulong UTF8::notherrng = sizeof(otherrng)/(sizeof(otherrng[0]*2));

const ushort UTF8::othersgl[] = 
{
	0x00aa,	/* ª */
	0x00b5,	/* µ */
	0x00ba,	/* º */
	0x03da,	/* ? */
	0x03dc,	/* ? */
	0x03de,	/* ? */
	0x03e0,	/* ? */
	0x06d5,	/* ? */
	0x09b2,	/* ? */
	0x0a5e,	/* ? */
	0x0a8d,	/* ? */
	0x0ae0,	/* ? */
	0x0b9c,	/* ? */
	0x0cde,	/* ? */
	0x0e4f,	/* ? */
	0x0e84,	/* ? */
	0x0e8a,	/* ? */
	0x0e8d,	/* ? */
	0x0ea5,	/* ? */
	0x0ea7,	/* ? */
	0x0eb0,	/* ? */
	0x0ebd,	/* ? */
	0x1fbe,	/* ? */
	0x207f,	/* n */
	0x20a8,	/* ? */
	0x2102,	/* C */
	0x2107,	/* E */
	0x2124,	/* Z */
	0x2126,	/* ? */
	0x2128,	/* Z */
	0xfb3e,	/* ? */
	0xfe74,	/* ? */
};

const ulong UTF8::nothersgl = sizeof(othersgl)/sizeof(othersgl[0]);

const ushort UTF8::spacerng[] =
{
	0x0009,	0x000a,	/* tab and newline */
	0x0020,	0x0020,	/* space */
	0x00a0,	0x00a0,	/*   */
	0x2000,	0x200b,	/*   - ? */
	0x2028,	0x2029,	/* -  */
	0x3000,	0x3000,	/*   */
	0xfeff,	0xfeff,	/* ? */
};

const ulong UTF8::nspacerng = sizeof(spacerng)/(sizeof(spacerng[0])*2);
}

char *MVTUtil::toLowerUTF8(const char *str,size_t ilen,uint32_t& olen,char *extBuf)
{
	const byte *in=(byte*)str,*end=in+(extBuf==NULL||ilen<olen?ilen:olen); byte ch;
	while (in<end && ((ch=*in)<'A' || ch>'Z' && ch<0x80)) in++;
	size_t l=in-(byte*)str; ulong wch; bool fAlc=false;
	if (extBuf==NULL) {
		//if (l>=ilen && alloc==PI_NO_ALLOC) return (char*)str;
		if ((fAlc=true,extBuf=(char*)::malloc(olen=uint32_t(ilen)))==NULL) return NULL;
	}
	if (l>0) memcpy(extBuf,str,l);
	while (in<end) {
		if ((ch=*in++)>='A' && ch<='Z') extBuf[l++]=ch+'a'-'A';
		else if (ch<0x80 || (wch=utf::UTF8::decode(ch,in,ilen-(in-(byte*)str)))==~0u) extBuf[l++]=ch;
		else {
			int lch=utf::UTF8::ulen(wch=utf::UTF8::towlower(wchar_t(wch)));
			if (l+lch>olen && (!fAlc || (extBuf=(char*)::realloc(extBuf,olen+=(olen<12?6:olen/2)))==NULL)) break;
			l+=utf::UTF8::encode((byte*)extBuf+l,wch);
		}
	}
	olen=uint32_t(l); return extBuf; 
}

char *MVTUtil::toUTF8(const wchar_t *ustr,size_t ilen,uint32_t& olen,char *str,bool fToLower)
{
	if (ustr==NULL || ilen==0 || str!=NULL && olen==0) {olen=0; return str;}
	bool fAlc=str==NULL; size_t l=0; ulong wch2=0;
	if (fAlc && ((str=(char*)::malloc(olen=uint32_t(ilen)/sizeof(wchar_t)))==NULL)) return NULL;
	for (size_t i=0; i<ilen; i+=sizeof(wchar_t)) {
		ulong wch=fToLower?utf::UTF8::towlower(*ustr++):*ustr++; int lch=utf::UTF8::ulen(wch);
		if (l+lch>olen) {
			if (!fAlc) break; size_t ll=l+lch,j=i; const wchar_t *p=ustr;
			while ((j+=sizeof(wchar_t))<ilen) {wch2=fToLower?utf::UTF8::towlower(*p++):*p++; ll+=utf::UTF8::ulen(wch);}
			if ((str=(char*)::realloc(str,olen=uint32_t(ll)))==NULL) break;
		}
		l+=utf::UTF8::encode((byte*)str+l,wch);
	}
	olen=uint32_t(l); return str;
}

wchar_t *MVTUtil::toUNICODE(const char *str,size_t ilen,uint32_t& olen,wchar_t *ustr)
{
	if (str==NULL || ilen==0 || ustr!=NULL && olen==0) {olen=0; return ustr;}
	bool fAlc=ustr==NULL; wchar_t *p=ustr; ulong wch;
	if (fAlc && ((p=ustr=(wchar_t*)::malloc(olen=uint32_t(ilen)*sizeof(wchar_t)))==NULL)) return NULL;
	for (const byte *in=(const byte*)str,*end=in+ilen; in<end && (byte*)p-(byte*)ustr+sizeof(wchar_t)<=olen; ) {
		byte ch=*in++; 
		if ((wch=utf::UTF8::decode(ch,in,end-in))!=~0u) *p++=wchar_t(wch);
		else {
			*p++=wchar_t(ch);	// ???
		}
	}
	size_t l=(byte*)p-(byte*)ustr;
	if (fAlc && l+l/2<olen) ustr=(wchar_t*)::realloc(ustr,l);
	olen=uint32_t(l); return ustr;
}

int MVTUtil::executeProcess(
	char const * pExe, 
	char const * pArgs,
	clock_t * outTimeTaken, // ms, optional
	TIMESTAMP * outTS,
	bool bLowPriority,
	bool bVerbose,
	bool bIgnoreSignal )
{
	if ( outTimeTaken ) { *outTimeTaken = 0 ; }
	if ( outTS ) { *outTS = 0 ; }
	clock_t lBef = 0, lAft = 0;
	TIMESTAMP lBefTS, lAftTS;
	#ifdef WIN32
		Tstring lCommand = pExe;
		lCommand += " ";
		lCommand += pArgs;		

		STARTUPINFO lStartupInfo;
		PROCESS_INFORMATION lProcessInfo;
		DWORD lExitCode = 0;
		::ZeroMemory(&lStartupInfo, sizeof(lStartupInfo));
		::ZeroMemory(&lProcessInfo, sizeof(lProcessInfo));
		lBef = getTimeInMs();getTimestamp(lBefTS);
		if (!::CreateProcess(NULL, (char *)lCommand.c_str(), NULL, NULL, TRUE, 0, NULL, NULL, &lStartupInfo, &lProcessInfo))
		{
			std::cout << "Could not execute '" << lCommand.c_str() << "'" << std::endl;
			return 1; 
		}
	
		if ( bLowPriority )
		{
			// Helps to avoid totally flooding the computer 
			::SetPriorityClass(lProcessInfo.hProcess,IDLE_PRIORITY_CLASS);
		}

		::WaitForSingleObject(lProcessInfo.hProcess, INFINITE);
		lAft = getTimeInMs(); getTimestamp(lAftTS);
		if ( outTimeTaken ) { *outTimeTaken = lAft-lBef; }
		if ( outTS ) { *outTS = lAftTS - lBefTS; }
		::GetExitCodeProcess(lProcessInfo.hProcess, &lExitCode);
		if (0 != lExitCode)
		{
			if(bIgnoreSignal)
			{
				std::cout << "'" << lCommand.c_str() << "' exited (code:" << lExitCode << " ignored)." << std::endl;
				lExitCode = 0;
			}
			else
				std::cout << "'" << lCommand.c_str() << "' failed (code:" << lExitCode << ")." << std::endl;
			return lExitCode;
		}
		else if ( bVerbose )
		{
			std::cout << "'" << lCommand.c_str() << "' succeeded" << std::endl;
		}
		return (int)lExitCode;
	#else	
		//Linux & Darwin 	
		lBef = getTimeInMs(); getTimestamp(lBefTS);	
		int const lForked = fork();
		if (0 == lForked)
		{					
			
			// Child process
			std::vector<char *> lArgs;
			lArgs.push_back((char *)pExe);
			if ( bVerbose ) 
				cout << "Child process " << pExe << " " << pArgs << endl ;
			char * lArgsCopy = NULL;
			if (pArgs && 0 != strlen(pArgs))
			{
				lArgsCopy = new char[strlen(pArgs) + 1];
				strcpy(lArgsCopy, pArgs);
				lArgs.push_back(lArgsCopy);
				while (*lArgsCopy)
				{
					if ('"' == *lArgsCopy)
					{
						lArgsCopy++;
						while (*lArgsCopy && '"' != *lArgsCopy)
							lArgsCopy++;
						lArgsCopy++;
					}
					else
					{
						bool lNext = false;
						while (' ' == *lArgsCopy)
						{
							*lArgsCopy++ = 0;
							lNext = true;
						}
						if (lNext && *lArgsCopy)
							lArgs.push_back(lArgsCopy);
						else
							lArgsCopy++;
					}
				}
			}
			lArgs.push_back(NULL);
			if (-1==execvp(pExe, &lArgs[0]))
			{
				cout << "execv failed" << endl;
				if (errno==ENOENT) cout<< "errno ENOENT"<<endl;
				else if (errno==EFAULT) cout<< "errno EFAULT"<<endl;
				else cout << "errno " << errno << endl;
				cout << "Exe " << pExe << endl;
				int ar=0;
				while(lArgs[ar]!=NULL) 
				{
					cout << "arg " << ar << " " << lArgs[ar] << endl;
					ar++;
				}
				char cwd[128];
				cout << "cwd " << getcwd(cwd, 128) << endl;
				return 1;	
			}
			cout << " I think I'll never see this... \n";
			lAft = getTimeInMs(); getTimestamp(lAftTS);
			if ( outTimeTaken ) { *outTimeTaken = lAft-lBef; }
			if ( outTS ) { *outTS = lAftTS - lBefTS; }
    
			delete [] lArgsCopy; // Review: Am I supposed to do this?
			return 0;
		}
		else if (-1 == lForked)
		{
			cout<<"Fork failed"<<endl;
			return 1;
		}
		else
		{
			// launching (parent) process
			if ( bVerbose ) 
				cout << "Launched child process " << lForked << endl;
			int lExitCode=0; pid_t ret;
			while ((ret=waitpid(lForked, &lExitCode, 0))==(pid_t)-1 && errno==EINTR) lExitCode=0;
			if (ret != lForked)
				std::cout << "Problem waiting for forked process!" << errno << std::endl;
			else
			{
				if(bIgnoreSignal && WIFSIGNALED(lExitCode))
				{	if(bVerbose) 
						psignal(WTERMSIG(lExitCode), "process terminated by a signal");
					lExitCode = 0;
				}
				if (bVerbose)
					cout << "process completed " << lExitCode << endl;
				lAft = getTimeInMs(); getTimestamp(lAftTS);			
				if ( outTimeTaken ) { *outTimeTaken = lAft-lBef; }
				if ( outTS ) { *outTS = lAftTS - lBefTS; }
				if(WIFSIGNALED(lExitCode))
					cout << "By signal:" << WTERMSIG(lExitCode) << endl;
			}
			cout << " Got exit code: " << lExitCode << endl;				
			return lExitCode;
		}
	#endif
}

void MVTUtil::mapURIs(ISession *pSession, const char *pPropName, int pNumProps, PropertyID *pPropIDs, const char *base)
{
	// MVTUtil::mapURIs is convenient for testing because it mixes 
	// a random string into the URI to ensure that the property ids are unique
	// each time the test is run.  This means a test doesn't need to flush its 
	// data out of the store and can be run in parallel

	char lB[100];	
	Tstring lPropStr;
	MVTRand::getString(lPropStr,10,10,false,true);
	URIMap lData;	
	int i = 0;
	for(i = 0; i < pNumProps; i++)
	{
		sprintf(lB, "%s%s.%d", pPropName, lPropStr.c_str(), i);
		lData.URI = lB; lData.uid = STORE_INVALID_URIID; 
		if(RC_OK!=pSession->mapURIs(1, &lData, base)) assert(false);
		pPropIDs[i] = lData.uid;	
	}
}


void MVTUtil::mapURIs(ISession *pSession, const char *pPropName, int pNumProps, URIMap *pPropMaps, const char *base)
{
	char lB[100];	
	Tstring lPropStr; MVTRand::getString(lPropStr,10,10,false,true);
	int i = 0;
	for(i = 0; i < pNumProps; i++)
	{
		sprintf(lB, "%s%s.%d", pPropName, lPropStr.c_str(), i);
		pPropMaps[i].URI = lB; pPropMaps[i].uid = STORE_INVALID_URIID; 
		if(RC_OK!=pSession->mapURIs(1, &pPropMaps[i],base)) assert(false);		
	}
}

PropertyID MVTUtil::getProp(ISession* inS, const char* inName)
{
	// Syntactic convenience for working with properties by name instead of property id
	// No random element is mixed in so it can be used to lookup an existing property
	URIMap pm ;
	pm.URI = inName ;
	if ( RC_OK == inS->mapURIs( 1, &pm ) )
		return pm.uid ;
	else
		return STORE_INVALID_URIID ;
}

PropertyID MVTUtil::getPropRand(ISession *pSession, const char *pPropName)
{
	// Basically just a convenient function for mapURIs with 1 prop
	// A random element is added to the required property name so that 
	// a unique property is created.  You can't use this function to do a lookup
	Tstring lPropStr; MVTRand::getString(lPropStr,10,10,false,true);
	Tstring lFullProp( pPropName ) ; lFullProp += lPropStr ;
	return getProp( pSession, lFullProp.c_str() ) ;
}

void MVTUtil::mapStaticProperty(ISession *pSession, const char *pPropName, URIMap &pPropMap, const char *base)
{
	pPropMap.URI = pPropName; pPropMap.uid = STORE_INVALID_URIID; 
	if(RC_OK!=pSession->mapURIs(1, &pPropMap, base)) assert(false);
}

void MVTUtil::mapStaticProperty(ISession *pSession, const char *pPropName, PropertyID &pPropID, const char *base)
{
	URIMap lData;
	lData.URI = pPropName; lData.uid = STORE_INVALID_URIID; 
	if(RC_OK!=pSession->mapURIs(1, &lData, base)) assert(false);
	pPropID = lData.uid;	
}

int MVTUtil::countPinsFullScan(ISession * pSession)
{
	CmvautoPtr<IStmt> lQ(pSession->createStmt());
	lQ->addVariable();
	uint64_t lCount;
	lQ->count(lCount);
	return int(lCount);
}
int MVTUtil::countPins(ICursor *result,ISession *session)
{
	int count=0;
	for (IPIN *pin; (pin=result->next())!=NULL; ){
		count++; pin->destroy(); pin = NULL;
	}
	return count;
}

size_t MVTUtil::getCollectionLength(Value const & pV)
{
	// REVIEW: perhaps argument should be a pointer instead of & to permit tests for the NULL case, which
	// can be considered the valid representation of an empty collection

	if ( &pV == NULL )
	{
		return 0 ; // Property completely missing
	}
	else if (VT_COLLECTION == pV.type)
	{
		if (pV.isNav())
		{
			size_t i = 0;
			Value const * lV = pV.nav->navigate(GO_FIRST);
			while (lV)
			{
				i++;
				lV = pV.nav->navigate(GO_NEXT);
			}

			// Review: count() method is more convenient
			// but doesn't hurt to also count the entries manually as well
			assert( i == pV.nav->count() ) ; 
			return i;
		}
		else
		{
			return pV.length ;
		}
	}
	else
 	{
		// This is a bit dubious, e.g. might be bug in the test, but could also be valid.
		// A property collection of size 1 is often represented just as the raw type,
		// in which case the "length" property is the size of the item itself, not the size of the collection
		return 1 ;
	}
}

bool MVTUtil::checkQueryPIDs( 
	IStmt * inQ,
	int cntExpected,
	const PID * inExpected,
	std::ostream & errorDetails,
	bool bVerbose,
	unsigned int inCntParams,
	const Value * inParams
) 
{
	// Confirm exactly what PIDs are returns in a query.  The order of PIDs in the input array is not important.
	// Duplicates, missing pids or extra pids will cause this function to return false

	bool bMatch = true ;

	// Take copy so we can change memory
	PID * searchList = (PID*)alloca(sizeof(PID)*cntExpected);
	memcpy(searchList,inExpected,sizeof(PID)*cntExpected) ;

	uint64_t cnt, enumcnt=0 ;
	inQ->count(cnt,inParams,inCntParams) ;
	if( (int)cnt != cntExpected ) 
	{
		errorDetails << "Count mismatch got " << cnt << " expected " << cntExpected << endl ;
		return false ;
	}

	PID rpid ;
	ICursor *result = NULL; 
	inQ->execute(&result, inParams,inCntParams);

	// Compare results	
	IPIN * rpin ;  
	while (rpin = result->next()) 
	{
		enumcnt++ ;
		rpid = rpin->getPID();
		if (bVerbose) { errorDetails << "\tFound " << rpid.pid << endl ; }
		rpin->destroy();
		bool bFound=false;
		for ( int i = 0 ; i < cntExpected ; i++ )
		{
			if ( searchList[i] == rpid )
			{
				bFound=true ;
				searchList[i].pid = STORE_INVALID_PID ; // Prevent duplicates from matching
				break;
			}
		}
		if (!bFound ) 
		{
			errorDetails << "Query PID not found in expected list " << std::hex << rpid << endl ;
			bMatch = false ;
		}
	}
	result->destroy();

	// Sanity check
	if (enumcnt!=cnt) {
		errorDetails << "ICursor didn't return as many pins as Count" << endl ;
		bMatch = false ;
	}

	return bMatch ;
}


bool MVTUtil::findDuplicatePins( IStmt * lQ, std::ostream & log  )
{
	// Returns true if any duplicate pins are found in the provided Query
	// log argument will be used to give debugging details about the problem when discovered

	int cntDupsFound = 0 ;

	uint64_t queryCount ;
	RC rc = lQ->count(queryCount) ;
	if ( rc != RC_OK ) { log << "Unable to get query count" << endl ; return false ; } 

	ICursor *lR = NULL;
       lQ->execute(&lR);
	if (lR == NULL)
	{
		assert( queryCount == 0 ) ;
		return false ;
	}

	/*** 
	 * In approach below the associative container multiset is going to be used. 
	 * multiset allows to have the multiple keys, which may be useful if we would like to traverse everything later.
	 * The keys are montained in sorted order, in our case  - ascending order.
	 * The dublicate considered found if at the moment of insertion, the key is already within multiset.
	 */ 
        typedef std::multiset<PID, less<PID> > foundpids;
        foundpids lFoundPIDs;
        foundpids::const_iterator result; 

        int pos = 0; 
     
        for(IPIN *lPIN = lR->next(); lPIN!=NULL; lPIN=lR->next(), pos++ )
	{					
		PID lPID = lPIN->getPID();
		
		result = lFoundPIDs.find(lPID);

		if( result != lFoundPIDs.end())
		{
			if ( cntDupsFound < 100 )
			{
				log << "testComplexFamily Duplicate PIN <<  " << std::hex << LOCALPID(lPID) 
					<< std::dec << " (appeared again at " << pos << ") "
					<< std::endl;
			}
			else if ( cntDupsFound == 100 )
			{
				log << "(only showing first 100 duplicates)" << std::endl ;
			}
			cntDupsFound++ ;
		}
		lFoundPIDs.insert(lPID);
		
		lPIN->destroy();
	}

	lR->destroy();

	// Sanity check		
	if(cntDupsFound == 0 && lFoundPIDs.size() != queryCount) 
	{
		log << "Mismatch between query count " << queryCount << " and enumeration count " << (ulong)lFoundPIDs.size() ;
		return false ;
	}
	return cntDupsFound > 0 ;
}

void MVTUtil::registerTestPINs(std::vector<PID> &pTestPIDs, PID *pPIDs,const int pNumPIDs){
	int i = 0;
	for(i = 0; i < pNumPIDs; i++)
		pTestPIDs.push_back(pPIDs[i]);
}

void MVTUtil::registerTestPINs(std::vector<IPIN *> &pTestPINs, IPIN **pPINs,const int pNumPINs){
	int i = 0;
	for(i = 0; i < pNumPINs; i++)
		pTestPINs.push_back(pPINs[i]);
}
void MVTUtil::unregisterTestPINs(std::vector<PID> &pTestPIDs,ISession *pSession){
	int i = 0;
	const int lNumPIDs = (int)pTestPIDs.size();	
	for(i = 0; i < lNumPIDs ; i++)
		if(RC_OK != pSession->deletePINs(&pTestPIDs[i],1,MODE_PURGE))
			std::cout << "ERROR (unregisterTestPINs): Failed to delete the Test PID " <<std::endl;
	pTestPIDs.clear();
}

void MVTUtil::unregisterTestPINs(std::vector<IPIN *> &pTestPINs,ISession *pSession){
	int i = 0;
	const int lNumPINs = (int)pTestPINs.size();
	for(i = 0; i < lNumPINs ; i++)
		if(RC_OK!=pSession->deletePINs(&pTestPINs[i],1,MODE_PURGE))
			std::cout << "ERROR (unregisterTestPINs): Failed to delete the Test PIN " <<std::endl;
	pTestPINs.clear();	
}

ClassID MVTUtil::getClass(ISession* inS, const char* inClass,uint32_t classnotify) 
{
	ClassID cls ;
	if ( RC_OK == inS->getClassID(inClass,cls) && RC_OK == inS->enableClassNotifications(cls,classnotify))
		return cls ;
	else
		return STORE_INVALID_CLASSID ;
}

ClassID MVTUtil::createUniqueClass(ISession* inS, const char* inPrefix, IStmt* inQ, std::string * outname /*optional*/,uint32_t classnotify/*optional */)
{
	RC rc; ClassID clsid = STORE_INVALID_CLASSID;

	if ( inS==NULL || inPrefix==NULL || inQ==NULL)
		return STORE_INVALID_CLASSID;

	char className[128];  assert(strlen(inPrefix)<100);
	do{	
		sprintf(className, "%s.%d", inPrefix, MVTRand::getRange(0,10000));
		rc = ITest::defineClass(inS, className, inQ, &clsid);
	}while(rc == RC_ALREADYEXISTS);	

	if (rc!=RC_OK)
		return STORE_INVALID_CLASSID;

	if (classnotify!=0) inS->enableClassNotifications(clsid,classnotify);

	if (outname!=NULL)
		*outname=className;

	return clsid;
}

//Encoding fucntions. adapted from Afy::utils.cpp and testosstringperfromance

/*
 * There is still open question within implementation below: 
 * Should the str be terminated by null, or not? 
 * The old implementation of the code assumed null termination. That is why 
 * it is implemented below. 
*/
char * MVTUtil::toUTF8(const wchar_t *ustr,uint32_t ilen,uint32_t& olen)
{
	char * str =MVTUtil::toUTF8(ustr,ilen, olen,NULL,false);
	str =(char *)::realloc(str, olen+1); str[olen] = 0;
	return str;
}
/*
 * There are at least 3 questions about the code implementation below: 
 * 1. Should the wchar_t * point to the null terminated string? 
 * 2. Should olen parameter include the terminating null?
 * 3. Should olen return the number of bytes, or a number of wchar_t characters?
 *
 * Presently, the code is as close as possible to the old implementation. 
 */
wchar_t * MVTUtil::toUNICODE(const char *str,uint32_t ilen,uint32_t& olen)
{
	wchar_t * ustr = (wchar_t *)::malloc((ilen+1)*sizeof(wchar_t));
	ustr[ilen] = (wchar_t)0;
	return toUNICODE(str,ilen,olen,ustr);
}

//Max's implementation of toUTF8 encoding

char * MVTUtil::myToUTF8(wchar_t const * pStr, size_t pLen, uint32_t & pBogus)
{
	size_t lLen = 1 + pLen;
	char * lRes = (char *)::malloc(lLen);
	char * lCur = lRes;
	for (size_t i = 0; (i < pLen) && (pStr[i] != 0); i++)
	{
		if (lCur - lRes + 7 > (long)lLen)
		{
			char * lORes = lRes;
			lLen = 1 + (lLen * pLen / i);
			lRes = (char *)::realloc(lRes, lLen);
			lCur += (lRes - lORes);
		}
		if (pStr[i] <= 0x0000007F) *lCur++ = char(pStr[i]);
		else if (pStr[i] <= 0x000007FF) { *lCur++ = 0xc0 | char(pStr[i] >> 6); *lCur++ = 0x80 | char(pStr[i] & 0x3F); }
		else if (pStr[i] <= 0x0000FFFF) { *lCur++ = 0xe0 | char(pStr[i] >> 12); *lCur++ = 0x80 | char((pStr[i] >> 6) & 0x3F); *lCur++ = 0x80 | char(pStr[i] & 0x3F); }
#ifndef WIN32
		else if (pStr[i] <= 0x001FFFFF) { *lCur++ = 0xf0 | char(pStr[i] >> 18); *lCur++ = 0x80 | char((pStr[i] >> 12) & 0x3F); *lCur++ = 0x80 | char((pStr[i] >> 6) & 0x3F); *lCur++ = 0x80 | char(pStr[i] & 0x3F); }
		else if (pStr[i] <= 0x03FFFFFF) { *lCur++ = 0xf8 | char(pStr[i] >> 24); *lCur++ = 0x80 | char((pStr[i] >> 18) & 0x3F); *lCur++ = 0x80 | char((pStr[i] >> 12) & 0x3F); *lCur++ = 0x80 | char((pStr[i] >> 6) & 0x3F); *lCur++ = 0x80 | char(pStr[i] & 0x3F); }
		else if (pStr[i] <= 0x7FFFFFFF) { *lCur++ = 0xfc | char(pStr[i] >> 30); *lCur++ = 0x80 | char((pStr[i] >> 24) & 0x3F); *lCur++ = 0x80 | char((pStr[i] >> 18) & 0x3F); *lCur++ = 0x80 | char((pStr[i] >> 12) & 0x3F); *lCur++ = 0x80 | char((pStr[i] >> 6) & 0x3F); *lCur++ = 0x80 | char(pStr[i] & 0x3F); }
#else
		assert(sizeof(wchar_t) == 2);
#endif
	}
	*lCur++ = 0;
	return lRes;
}

void MVTUtil::getCurrentTime(Tstring & pTime)
{
	char lTimeStr[13];
	#ifdef WIN32
		__time64_t lTime;
		struct __timeb64 lStruct;
		struct tm *lGMT;
		_time64(&lTime);
		lGMT = _gmtime64(&lTime); _ftime64(&lStruct);
		sprintf(lTimeStr, "%02d:%02d:%02d %03d",lGMT->tm_hour, lGMT->tm_min, lGMT->tm_sec, lStruct.millitm );
	#else
		int lMS;
		struct timeval lTV;
		struct tm *lGMT;
		gettimeofday (&lTV, NULL);
		lGMT = localtime (&lTV.tv_sec);
		lMS = lTV.tv_usec / 1000;
		sprintf(lTimeStr, "%02d:%02d:%02d %03d",lGMT->tm_hour, lGMT->tm_min, lGMT->tm_sec, lMS );		
	#endif
	pTime = lTimeStr;	
}

bool MVTUtil::equal(IPIN const & p1, IPIN const & p2, ISession & pSession, bool pIgnoreEids)
{
	Md5Stream lS1, lS2;
	unsigned char lM1[16], lM2[16];
	long const lCmpFlags = pIgnoreEids ? MvStoreSerialization::ContextOutComparisons::kFExcludeEids : 0;
	MvStoreSerialization::ContextOutComparisons lSerCtx1(lS1, pSession, lCmpFlags), lSerCtx2(lS2, pSession, lCmpFlags);
	MvStoreSerialization::OutComparisons::properties(lSerCtx1, p1); lS1.flush_md5(lM1);
	MvStoreSerialization::OutComparisons::properties(lSerCtx2, p2); lS2.flush_md5(lM2);
	return (0 == memcmp(lM1, lM2, sizeof(lM1) / sizeof(lM1[0])));
}
bool MVTUtil::equal(Value const & pVal1, Value const & pVal2, ISession & pSession, bool pIgnoreEids)
{
	Md5Stream lS1, lS2;
	unsigned char lM1[16], lM2[16];
	long const lCmpFlags = pIgnoreEids ? MvStoreSerialization::ContextOutComparisons::kFExcludeEids : 0;
	MvStoreSerialization::ContextOutComparisons lSerCtx1(lS1, pSession, lCmpFlags), lSerCtx2(lS2, pSession, lCmpFlags);
	MvStoreSerialization::OutComparisons::value(lSerCtx1, pVal1); lS1.flush_md5(lM1);
	MvStoreSerialization::OutComparisons::value(lSerCtx2, pVal2); lS2.flush_md5(lM2);
	return (0 == memcmp(lM1, lM2, sizeof(lM1) / sizeof(lM1[0])));
}

// Outputing/logging/other services.
void MVTUtil::output(Value const & pV, std::ostream & pOs, ISession * pSession)
{
	MvStoreSerialization::ContextOutDbg lSerCtx(pOs, pSession, 64, MvStoreSerialization::ContextOutDbg::kFShowPropIds); // kFRecurseRefs is much too verbose
	MvStoreSerialization::OutDbg::value(lSerCtx, pV);
}

void MVTUtil::output(IPIN const & pPIN, std::ostream & pOs, ISession * pSession)
{
	MvStoreSerialization::ContextOutDbg lSerCtx(pOs, pSession, 64, MvStoreSerialization::ContextOutDbg::kFShowPropIds); // kFRecurseRefs is much too verbose);
	MvStoreSerialization::OutDbg::pin(lSerCtx, pPIN);
}

void MVTUtil::output(const PID & pid, std::ostream & pOs, ISession * pSession)
{
	// Convenience 
	IPIN * pin = pSession->getPIN( pid ) ;
	if ( pin )
	{
		output( *pin, pOs, pSession ) ;
		pin->destroy() ;
	}
	else
	{
		pOs << "invalid pid!" << endl ;
	}
}

void MVTUtil::output(const IStoreNotification::NotificationEvent & event, std::ostream & pOs, ISession* inSession)
{
	// For debugging purposes, view the contents of a NotificationEvent

	static const char * lEventTypes[] = { "NE_PIN_CREATED", "NE_PIN_UPDATED", "NE_PIN_DELETED", "NE_CLASS_INSTANCE_ADDED", "NE_CLASS_INSTANCE_REMOVED", "NE_CLASS_INSTANCE_CHANGED", "NE_PIN_UNDELETE" } ; // Keep in sync with NotificationEventType in startup.h

	pOs << "PID " << std::hex << event.pin.pid << "," << event.pin.ident << endl ;
	pOs << "fReplication " << event.fReplication << endl ;
	pOs << "nEvents " << event.nEvents << endl ;

	size_t i ;
	for ( i = 0 ; i < event.nEvents ; i++ )
	{
		pOs << "\ttype:" <<  lEventTypes[event.events[i].type]  
			<< " cid: " << std::hex << event.events[i].cid  << endl;
	}

	pOs << "nData " << event.nData << endl ;
	for ( i = 0 ; i < event.nData ; i++ )
	{
		pOs << "\tProperty: " << std::dec << event.data[i].propID 
			<< " eid:" << std::hex << event.data[i].eid << std::dec;

		pOs << " epos:" << std::hex << event.data[i].epos << std::dec << endl ;

		if ( event.data[i].oldValue != NULL )
		{
			pOs << "\tOld value: " ; 
			output( *(event.data[i].oldValue), pOs, inSession ) ;
		}

		if ( event.data[i].newValue != NULL )
		{
			pOs << "\tNew value: " ;
			output( *(event.data[i].newValue), pOs, inSession ) ;
		}

		pOs << endl ;
	}
}

void MVTUtil::ensureDir( const char * inDir ) 
{
// Ensure directory exists 
#ifdef WIN32
	if ( GetFileAttributes( inDir ) == INVALID_FILE_ATTRIBUTES )
	{
		string lCmd = "/C mkdir " ;
		lCmd += inDir ;
		MVTUtil::executeProcess("cmd.exe", lCmd.c_str());
	}
#else
	if ( 0!= mkdir(inDir,S_IRWXU|S_IRWXG|S_IRWXO) )
	{
		if ( errno != EEXIST )
			std::cout << "Fail to create directory " << inDir << " errno:" << errno << endl;
	}

	#if 0 
		//execute a conditional dir creation like 
		//if ! [ -d t ] ; then mkdir t ; fi

		string lCmd = "bash -c \"if ! [ -e " ;
		lCmd += inDir ;
		lCmd += " ] ; then mkdir " ;
		lCmd += inDir ;
		lCmd += " ; fi\"" ;
		system(lCmd.c_str());
	#endif
#endif
}

void MVTUtil::backupStoreFiles(
	const char * inStoreDirNoTrailingSlash, 
	const char * inDestDir )
{
	// Take a snapshot of .mvlog and .pidata into a subdirectory

	string lDir( "." );
	string lCmd ;

	if ( inStoreDirNoTrailingSlash != NULL )
	{
		lDir = inStoreDirNoTrailingSlash ;
	}

#ifdef WIN32
	string lDestDir( ".\\backup" );
#else
	string lDestDir( "./backup" );
#endif
	if ( inDestDir != NULL )
	{
		lDestDir = inDestDir ;
	}

#ifdef WIN32
	if ( GetFileAttributes( lDestDir.c_str() ) == INVALID_FILE_ATTRIBUTES )
	{
		lCmd = "/C mkdir " ;
		lCmd += lDestDir ;
		MVTUtil::executeProcess("cmd.exe", lCmd.c_str());
	}
	lCmd = "/C del " ;
	lCmd += lDestDir ;
	lCmd += "\\" STOREPREFIX "*.* " ;
	lCmd += lDestDir ;
	lCmd += "\\" LOGPREFIX "*.* " ;
	MVTUtil::executeProcess("cmd.exe", lCmd.c_str());

	lCmd = "/C copy /Y " ;
	lCmd += lDir ;
	lCmd += "\\" STOREPREFIX "*" DATAFILESUFFIX " " ;
	lCmd += lDestDir ;
	MVTUtil::executeProcess("cmd.exe", lCmd.c_str());

	lCmd = "/C copy /Y " ;
	lCmd += lDir ;
	lCmd += "\\" LOGPREFIX "*" LOGFILESUFFIX " " ;
	lCmd += lDestDir ;
	MVTUtil::executeProcess("cmd.exe", lCmd.c_str());

	//TODO if needed?
	//lCmd += "\\rep_*.dat " ;
#else

	// Todo - rewrite with native file i/o API to avoid 
	// extra warning messages

	lCmd = "bash -c \"rm -rf " ;
	lCmd += lDestDir ;
	lCmd += "\"" ;
	if (-1 == system(lCmd.c_str()))
		{ assert(false); }

	lCmd = "bash -c \"mkdir " ;
	lCmd += lDestDir ;
	lCmd += "\"" ;
	if (-1 == system(lCmd.c_str()))
		{ assert(false); }

	lCmd = "bash -c \"cp " ;
	lCmd += lDir ;
	lCmd += "/" STOREPREFIX "*" DATAFILESUFFIX " " ;
	lCmd += lDestDir ;
	lCmd += "\"" ;
	if (-1 == system(lCmd.c_str()))
		{ assert(false); }

	lCmd = "bash -c \"cp " ;
	lCmd += lDir ;
	lCmd += "/" STOREPREFIX "*" LOGFILESUFFIX " " ;
	lCmd += lDestDir ;
	lCmd += "\"" ;
	if (-1 == system(lCmd.c_str()))
		{ assert(false); }
#endif
}

bool MVTUtil::deleteStoreFiles(const char* inDir)
{
	//
	// THIS FUNCTION IS BEING PHASED OUT BY MVTApp::deleteStore
	// because that version supports s3io and is cleaner implementation.
	// However until that can support arbitraty store directories
	// this low level utility will be used by some tests
	//

	std::cout << "Deleting store in dir  " << inDir << " (old method)" << endl;

	bool bRetVal = true ;


	// This can only pass if the store is not actively open.  
	// Normal tests should not call it
	// so that they can be run in parallel.

	// Goal is to succeed silently if the store files are already missing
	// but some more work is needed first

	string lDir ;
	string lCmd ;

	if ( inDir != NULL )
	{
		lDir = inDir ;
#ifdef WIN32
		lDir += "\\" ;
#else
		lDir += "/" ;
#endif
	}

	string pathDatFile = lDir ;
	pathDatFile += STOREPREFIX DATAFILESUFFIX ;

	#ifdef WIN32
		//Note: this avoids deleting Afy*.dll!
		if ( INVALID_FILE_ATTRIBUTES != ::GetFileAttributes(pathDatFile.c_str()) )
		{
			bRetVal = 0 != ::DeleteFile(pathDatFile.c_str());

			if ( !bRetVal ) 
			{
				cout << "Failed to delete " << pathDatFile << " error: " << GetLastError() << endl ;
			}
		}

		if ( bRetVal )
		{
			// REVIEW:  we only delete the logs if the store data file was missing or successfully
			// deleted.  If it is still open then deleting the log files can cause
			// scary errors sooner or later from the store.

			// Also any log files (left if store crashed)
			lCmd = "/C if EXIST " ;
			lCmd += lDir ;
			lCmd += "Afy*.mv*"; // Catches LOGFILESUFFIX, but also s3io files like pirmap, piwmap
			lCmd += " ( del " ;
			lCmd += lDir ;
			lCmd += "Afy*.mv*";
			lCmd += " )" ;
			MVTUtil::executeProcess("cmd.exe", lCmd.c_str());

		}
	#else
		if (-1 == unlink(pathDatFile.c_str()))
		{
			bRetVal = false;
			stringstream err ;
			switch (errno)
			{
				case EFAULT : 
				case ENOENT :
					bRetVal = true ; break ; /* file doesn't exist, ignore */
				case EACCES : err << "EACCES" ; break ; 
				case EBUSY : err << "EBUSY" ; break ; 
				case EINTR : err << "EINTR" ; break ; 
				case EROFS : err << "EROFS (read-only)" ; break ;
				default : err << "errno = " << errno ; break ;
			}
			if ( !bRetVal )
			{
				cout << "Error deleting " << pathDatFile << ":" << err.str() << endl ;
			}
		}

		if ( bRetVal )
		{
			// -f means silent if file not present
			lCmd = "bash -c \"rm -f " ;
			lCmd += lDir ;
			lCmd += "Afy*.mv*"; // Catches LOGFILESUFFIX, but also s3io files like pirmap, piwmap
			lCmd += "\"" ;
			if (-1 == system(lCmd.c_str()))
			  { assert(false); }
		}
	#endif

	if ( bRetVal )
		removeReplicationFiles(lDir.c_str());

	return bRetVal ;
}

#if !defined(WIN32) && !defined(Darwin)
#include <dirent.h>
#include <fnmatch.h>
#endif

bool MVTUtil::deleteStore(const char * inIOInit, const char * inStoreDir, const char * inLogDir, bool inbArchiveLogs)
{
	// New version using the IO layer to delete the files
	// This makes it possible for s3 to also delete its files.
	// It respects the settings of the currently active test suite,
	// so it should work for multi-stores.

	std::cout << "Deleting store in dir " << inStoreDir << endl;

#ifdef Darwin        
        string pathDat = inStoreDir ;
        pathDat += "/" ;
        string rm = "rm " +  pathDat + "affinity.store; rm -rf " + pathDat + "*.txlog";
        
	//int ersh = system( "rm mv.store; rm -rf *.txlog;");
	int ersh = system( rm.c_str());
	std::cout << rm.c_str() << " ( " << ersh << ")" << endl;
        return true; 
#else

	string pathDat = inStoreDir ;

#ifdef WIN32
	pathDat += "\\" ;
#else
	pathDat += "/" ;
#endif

	string datFile = pathDat + STOREPREFIX DATAFILESUFFIX ;

#ifdef WIN32
	::DeleteFile(datFile.c_str());
#else
	unlink(datFile.c_str());
#endif

	string logDir = inStoreDir ;
	if ( inLogDir != NULL && *inLogDir != '\0' ) {
		logDir = inLogDir;
	}
	#ifdef WIN32
		logDir += "\\";
	#else
		logDir += "/";
	#endif	

#ifdef WIN32
	string mask = logDir + LOGPREFIX"*"LOGFILESUFFIX;
	WIN32_FIND_DATA findData; HANDLE h=FindFirstFile(mask.c_str(),&findData);
	if (h!=INVALID_HANDLE_VALUE) {
		do {string fname = logDir + findData.cFileName; ::DeleteFile(fname.c_str());} while (FindNextFile(h,&findData)==TRUE);
		FindClose(h);
	}
#else
	DIR *dirP=opendir(logDir.c_str());
	if (dirP!=NULL) {
		struct dirent *ep; char buf[PATH_MAX+1]; char *end;
		while ((ep=readdir(dirP))!=NULL) if (fnmatch(LOGPREFIX"*"LOGFILESUFFIX,ep->d_name,FNM_PATHNAME|FNM_PERIOD)==0) {
			if (inbArchiveLogs) {
				// ???
			}
			else {
				string fname = logDir + ep->d_name; unlink(fname.c_str());
			}
		}
		closedir(dirP);
	}
#endif

	removeReplicationFiles(pathDat.c_str());
	return true;
#endif
}

bool MVTUtil::removeReplicationFiles(const char * inPathWithSlash)
{
	// When a store is deleted any replication files should also be erased
	string lCmd;

	#ifdef WIN32
		// Replication related files
		lCmd = "/C if EXIST " ;
		lCmd += inPathWithSlash ;
		lCmd += "rep_*.dat ( del " ;
		lCmd += inPathWithSlash ;
		lCmd += "rep_*.dat )" ;
		MVTUtil::executeProcess("cmd.exe", lCmd.c_str());
	#else
		// -f means silent if file not present
		lCmd = "bash -c \"rm -f " ;
		lCmd += inPathWithSlash ;
		lCmd += "rep*"; // Catches LOGFILESUFFIX, but also s3io files like pirmap, piwmap
		lCmd += "\"" ;
		if (-1 == system(lCmd.c_str()))
			{ assert(false); }
	#endif

	return true;
}

string MVTUtil::completeMultiStoreIOInitString(const string & strConfig, int storeIndex)
{
	// Produce a final version of the ioinit string
	//
	// Initially this means replacing %index% in the ioinit string with the actual
	// store index.  It %index% token is not present then no change is made to the
	// ioinit string
	//
	// Based on code from ioconfig.h - 
	// we may want to generalized this for other token replacement

	const char* pcStart = strConfig.c_str();
	std::stringstream strFinalConfig;
	while ( pcStart != NULL)
	{
		const char* pcTokenBegin = strchr(pcStart,'%');
		if( pcTokenBegin != NULL)
		{
			const char* pcTokenEnd =NULL;
	    
			//Copy the data till the place holder
			strFinalConfig << std::string(pcStart,pcTokenBegin-pcStart);
	    
			//Begin found but no end then error condition ignore the config
			pcTokenEnd = strchr(pcTokenBegin+1,'%');
			if( pcTokenEnd == NULL)
			{
				return "";
			}
			else
			{
				std::string strPlaceHolderName(pcTokenBegin+1,pcTokenEnd-pcTokenBegin-1);
				if( strPlaceHolderName == "index")
				{
					strFinalConfig << storeIndex; 
				}
				pcStart = pcTokenEnd+1;
			}
		}
		else
		{
			strFinalConfig << pcStart;
			break;
		}
	}
	return strFinalConfig.str();
}

RC  MVTUtil::getHostname(char* hostname){
    RC rc = RC_OK;
    char dirbuf[50];
    if (hostname==NULL) return RC_INVPARAM;
#ifdef WIN32
    DWORD lbuf=sizeof(dirbuf);
    if (!::GetComputerName(dirbuf,&lbuf)) rc=convCode(GetLastError());
#else
    if (gethostname(dirbuf,sizeof(dirbuf))) rc=convCode(errno);
#endif
    if (rc!=RC_OK) {printf("%s", "Cannot get host name (%d)\n",rc); return rc;}
    strncpy(hostname, dirbuf, strlen(dirbuf));return rc;
}