/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include "mvauto.h"
#include <iomanip>
#include <limits>
using namespace std;

#define TEST_EXECUTE_GOOD_STMT         1 // to actually execute any statement that parses.
#define TEST_PRECREATE_GOOD_CLASSES    1 // to populate the db with good classes.
#define TEST_PRECREATE_GOOD_DATA       1 // to populate the db with good data.
#define TEST_PRECREATE_INCOHERENT_DATA 1 // to populate the db with data that departs from the schema's conventions.
#define TEST_MEANINGLESS_FULL          1 // to test completely random arrangements of keywords.
#define TEST_MEANINGLESS_PARTIAL_A     1 // to test semi-coherent arrangements of keywords (containing many faults).
#define TEST_MEANINGLESS_PARTIAL_B     1 // to test semi-coherent arrangements of keywords (containing hopefully just 1 fault).
#define TEST_MEANINGLESS_PARTIAL_C     1 // to test semi-coherent arrangements of keywords (many many faults - almost unrecognizable).
#define TEST_ISSUE_DOUBLE_QUOTED_VAL   0 // to include/skip a known crash in INSERT with "value".
#define TEST_INSERT                    1 // to include/skip all INSERT statements.
#define TEST_SELECT                    1 // to include/skip all SELECT statements.
#define TEST_UPDATE                    1 // to include/skip all UPDATE statements.
#define TEST_DELETE                    1 // to include/skip all DELETE statements.

// Publish this test.
class TestMeanSql : public ITest
{
	public:
		TEST_DECLARE(TestMeanSql);
		virtual char const * getName() const { return "TestMeanSql"; }
		virtual char const * getHelp() const { return ""; }
		virtual char const * getDescription() const { return "tries to be mean with pathSQL parser (make it crash)"; }
		virtual bool includeInSmokeTest(char const *& pReason) const { return false; }
		virtual int execute();
		virtual void destroy() { delete this; }
		void testStmt(ISession * pSession, Tstring const & pStmt)
		{
			if (isKnownFailure(pStmt))
				return;
			mLogger.out() << "tried: " << pStmt.c_str() << std::endl;
			CompilationError lCE;
			CmvautoPtr<IStmt> lStmt(pSession->createStmt(pStmt.c_str(), NULL, 0, &lCE));
			mLogger.out() << "obtained: " << lCE.rc << " " << (lCE.msg ? lCE.msg : "") << std::endl << std::endl;
			#if TEST_EXECUTE_GOOD_STMT
				if (RC_OK == lCE.rc && lStmt.IsValid())
					lStmt->execute();
			#endif
		}
	protected:
		bool isKnownFailure(Tstring const & pStmt) const { for (size_t i = 0; i < mNumKnownFailures; i++) { if (Tstring::npos != pStmt.find(sKnownFailures[i])) return true; } return false; }
		void testRandomStatement(ISession & pSession, size_t pMinWords, size_t pMaxWords);
		static size_t countArray(char const * const * pArray) { size_t i = 0; while (pArray[i++]); return i - 1; }
	protected:
		struct GenCtx
		{
			double mRatioOfGood; // Desired probability, in [0, 1.0], of 'good' elements in the built query.
			size_t mMaxBad, mNumBad; // Optional maximum number of 'bad' elements per built query.
			size_t mProduced; // Tmp return value for 'gen_' calls that want to communicate aspects of their internal decisions.
			bool mShort; // Try to keep it short (e.g. smaller enumerations/lists/...).
			GenCtx(double pRatioOfGood, bool pShort=true, size_t pMaxBad=0) : mRatioOfGood(pRatioOfGood), mMaxBad(pMaxBad), mNumBad(0), mProduced(0), mShort(pShort) {}
			bool doGood()
			{
				if (mMaxBad > 0 && mNumBad >= mMaxBad) return true;
				bool const lDoGood = (mRatioOfGood >= 1.0 || MVTRand::getDoubleRange(0, 1.0) <= mRatioOfGood);
				if (!lDoGood) mNumBad++;
				return lDoGood;
			}
		};
		Tstring & gen_matching(GenCtx & pCtx, Tstring & pResult, char pOpening);
		Tstring & gen_label(GenCtx & pCtx, Tstring & pResult);
		Tstring & gen_value(GenCtx & pCtx, Tstring & pResult);
		Tstring & gen_collection(GenCtx & pCtx, Tstring & pResult);
		Tstring & gen_assignment(GenCtx & pCtx, Tstring & pResult);
		Tstring & gen_projection(GenCtx & pCtx, Tstring & pResult, bool pWithAs);
		Tstring & gen_INSERT_newstyle(GenCtx & pCtx, Tstring & pResult);
		Tstring & gen_INSERT_oldstyle(GenCtx & pCtx, Tstring & pResult);
		Tstring & gen_UPDATE(GenCtx & pCtx, Tstring & pResult);
		Tstring & gen_DELETE(GenCtx & pCtx, Tstring & pResult);
		Tstring & gen_CLASS(GenCtx & pCtx, Tstring & pResult);
		Tstring & gen_SELECT(GenCtx & pCtx, Tstring & pResult);
		Tstring & gen_statement(GenCtx & pCtx, Tstring & pResult);
		Tstring & gen_whatever(GenCtx & pCtx, Tstring & pResult);
		Tstring randomWord() { return mAllWords[MVTRand::getRange(0, mNumWords - 1)]; }
		Tstring const & randomProperty() { return mProperties[MVTRand::getRange(0, kpTOTAL - 1)]; } // TODO: qnames, quoting etc.
		Tstring const & randomClass() { return mClasses[MVTRand::getRange(0, kcTOTAL - 1)]; } // TODO: qnames, quoting etc.
		Tstring randomKeyword() { return sKeywords[MVTRand::getRange(0, mNumKeywords - 1)]; }
		Tstring randomInfixOperator() { return sInfixOperators[MVTRand::getRange(0, mNumInfixOperators - 1)]; }
		Tstring randomInfixComparator() { return sInfixComparators[MVTRand::getRange(0, mNumInfixComparators - 1)]; }
	protected:
		static char const * const sKnownFailures[];
		static char const * const sKeywords[];
		static char const * const sInfixOperators[];
		static char const * const sInfixComparators[];
		static char const * const sRadixProp[];
		static char const * const sRadixClass[];
		size_t mNumKnownFailures;
		size_t mNumKeywords;
		size_t mNumInfixOperators;
		size_t mNumInfixComparators;
	protected:
		enum eProperties { kpName = 0, kpAge, kpProfession, kpFriend, kpTag, kpOwner, kpCar, kpCarmake, kpColor, kpYear, kpTOTAL };
		enum eClasses { kcPeople, kcCars, kcTags, kcTOTAL };
		Tstring mProperties[kpTOTAL];
		Tstring mClasses[kpTOTAL];
		size_t mNumWords;
		char const ** mAllWords;
};
TEST_IMPLEMENT(TestMeanSql, TestLogger::kDStdOut);
char const * const TestMeanSql::sKnownFailures[] = // Note: Used to skip known failures, or at least to track them.
{
	"SELECT @",
	"SELECT INTERSECT SELECT ORDER", // also SELECT UNION SELECT ... etc.
	"SELECT LANG . *",
	"SELECT age_xCktrtjctbYVutGw . NOT SAMETERM year_xCktrtjctbYVutGw PURGE MOVE FIRST . PURGE FROM STR ISURI ADD INNER year_xCktrtjctbYVutGw CURRENT_STORE OR UPDATE WITH LAST PART @ TRANSACTION AS PREFIX",
	"SELECT people_nsslgSXiPcqXETZDxllf . * START A : ",
	"SELECT DISTINCT @ LEFT MAX people_pVRLLqDDohFlRFt REDUCED * CLOSE GROUP CLOSE car_pVRLLqDDohFlRFt ISIRI DELETE WRITE tag_pVRLLqDDohFlRFt EDIT color_pVRLLqDDohFlRFt MIN ISIRI COMMIT NULL CLOSE SOME AFTER OPEN A :",
	"SELECT year_xVwxWBTERioz,@ AS ISLITERAL GROUP BY profession_xVwxWBTERioz,\"name_xVwxWBTERioz\",car_xVwxWBTERioz,\"age_xVwxWBTERioz\",\"name_xVwxWBTERioz\"",
	"INSERT (age_ASKgHzHQFc,owner_ASKgHzHQFc,owner_ASKgHzHQFc,owner_ASKgHzHQFc) VALUES (TRUE,{BOUND,'TnQbBzdiTWaUAPiWBI K','SHJJLV ',1864861332},5.02194e+307,{'WLWjEiGltSuYDsJK',TRUE,1.19737e+308,'VbIjLJIbUKRBVvoWE ',2.39712e+38f,2.97478e+307,'QfZSKaQrPEsffY'})",
	"INSERT (\"color_loMzAIUmuKixFAyL\",tag_loMzAIUmuKixFAyL,\"color_loMzAIUmuKixFAyL\",tag_loMzAIUmuKixFAyL) VALUES (FALSE,660133041,1.45672e+308,{9.50291e+307,1221485048,AFTER,4.04379e+307,3.06605e+37f,811616852,3.12513e+307,FALSE,'PnEMbM'})",
	"INSERT profession_JaVDJNqoiCTxWtBe=2145339483,color_JaVDJNqoiCTxWtBe='sZABuocP',color_JaVDJNqoiCTxWtBe=19918836,car_JaVDJNqoiCTxWtBe=619276228,name_JaVDJNqoiCTxWtBe={3.80456e+307,1736889285},color_JaVDJNqoiCTxWtBe={FALSE,1862930862}",
	"SELECT \"name_iItCtsbDrjyUbMwDQ\",friend_iItCtsbDrjyUbMwDQ AS VqNylafrXSGbDrrVuWZ,year_iItCtsbDrjyUbMwDQ,year_iItCtsbDrjyUbMwDQ,year_iItCtsbDrjyUbMwDQ AS Ymwvliji,carmake_iItCtsbDrjyUbMwDQ AS fLvLEaExmkWkfKbkZ,carmake_iItCtsbDrjyUbMwDQ AS kvChAlSM,\"owner_iItCtsbDrjyUbMwDQ\" AS uarnaHeazSmofkvsqi,\"owner_iItCtsbDrjyUbMwDQ\" AS MkODvdo,color_iItCtsbDrjyUbMwDQ AS EYfCeJSvma WHERE (friend_iItCtsbDrjyUbMwDQ>FALSE) GROUP BY \"age_iItCtsbDrjyUbMwDQ\",\"owner_iItCtsbDrjyUbMwDQ\",tag_iItCtsbDrjyUbMwDQ,friend_iItCtsbDrjyUbMwDQ,\"tag_iItCtsbDrjyUbMwDQ\",carmake_iItCtsbDrjyUbMwDQ", // this one freezed
	"SELECT \"year_JmGlMggPtDwAMdhAv\",name_JmGlMggPtDwAMdhAv AS fCYulBFCBp,tag_JmGlMggPtDwAMdhAv,friend_JmGlMggPtDwAMdhAv,tag_JmGlMggPtDwAMdhAv,\"age_JmGlMggPtDwAMdhAv\",\"friend_JmGlMggPtDwAMdhAv\" GROUP BY \"color_JmGlMggPtDwAMdhAv\",\"carmake_JmGlMggPtDwAMdhAv\",\"car_JmGlMggPtDwAMdhAv\",car_JmGlMggPtDwAMdhAv,color_JmGlMggPtDwAMdhAv,friend_JmGlMggPtDwAMdhAv ORDER BY year_JmGlMggPtDwAMdhAv",
	"SELECT \"carmake_asnZQKemefntMeYinQCc\" AS jBNOdPGgUJy,name_asnZQKemefntMeYinQCc AS QgbpbcTPJLhwPcQOBuI,\"car_asnZQKemefntMeYinQCc\",\"carmake_asnZQKemefntMeYinQCc\",color_asnZQKemefntMeYinQCc AS JtppXyIJZMDkgUCSULH,\"car_asnZQKemefntMeYinQCc\",owner_asnZQKemefntMeYinQCc AS hjdqlsD FROM people_asnZQKemefntMeYinQCc AS a JOIN tags_asnZQKemefntMeYinQCc AS b ON (a.profession_asnZQKemefntMeYinQCc=b.carmake_asnZQKemefntMeYinQCc) GROUP BY friend_asnZQKemefntMeYinQCc ORDER BY car_asnZQKemefntMeYinQCc",
	"SELECT carmake_LNuYMKpMzFNja AS fgJBo,\"car_LNuYMKpMzFNja\",\"owner_LNuYMKpMzFNja\" AS jRJbSEPRo ORDER BY name_LNuYMKpMzFNja", // /media/truecrypt1/src/kernel/src/utils.h:361: void AfyKernel::HChain<T>::insertFirst(AfyKernel::HChain<T>*) [with T = AfyKernel::ObjName]: Assertion `obj==__null&&elt->obj!=__null' failed.
	"SELECT \"color_awojgBvzDsrjuVslt\" AS HifZASgrhDZOW,profession_awojgBvzDsrjuVslt,\"profession_awojgBvzDsrjuVslt\",\"name_awojgBvzDsrjuVslt\" AS EhKOvFo,color_awojgBvzDsrjuVslt AS rfvemE,tag_awojgBvzDsrjuVslt AS EEUWX GROUP BY name_awojgBvzDsrjuVslt,\"car_awojgBvzDsrjuVslt\",profession_awojgBvzDsrjuVslt,\"tag_awojgBvzDsrjuVslt\",owner_awojgBvzDsrjuVslt,\"tag_awojgBvzDsrjuVslt\",owner_awojgBvzDsrjuVslt,\"friend_awojgBvzDsrjuVslt\"", // /media/truecrypt1/src/kernel/src/dlalloc.cpp:175: void do_check_inuse_chunk(malloc_state*, malloc_chunk*): Assertion `((((mchunkptr)(((char*)(p))+((p)->size & ~0x1)))->size) & 0x1)' failed.
	"INSERT name_BntALVdSYiGxuOIkHjIH=X'4674546f77',car_BntALVdSYiGxuOIkHjIH='nOtwlSRc',name_BntALVdSYiGxuOIkHjIH=@2a4426,car_BntALVdSYiGxuOIkHjIH=carmake_BntALVdSYiGxuOIkHjIH=2.84075e+307",
	"INSERT age_UdTQlLMBSZ={CURRENT_TIMESTAMP,U'http://oIwre',{@49e5ed.name_UdTQlLMBSZ[51]}}",
	"SELECT people_lmcpogYSKENgiK FROM people_lmcpogYSKENgiK WHERE (car_lmcpogYSKENgiK<>${ORDER}) GROUP BY \"tag_lmcpogYSKENgiK\",*,",
	"SELECT {U'http://aEnYa',@397412.age_yBRgesudEDnq[:FIRST],${SELECT * GROUP BY profession_yBRgesudEDnq,\"name_yBRgesudEDnq\",age_yBRgesudEDnq,\"name_yBRgesudEDnq\",color_yBRgesudEDnq,carmake_yBRgesudEDnq,owner_yBRgesudEDnq,tag_yBRgesudEDnq,profession_yBRgesudEDnq,\"year_yBRgesudEDnq\"},INTERVAL'559394531:25:0.12345',@4ed029.carmake_yBRgesudEDnq,'SdXxTaCnRpacqGs'},*,tag_yBRgesudEDnq WRITE HcE,OPEN FILTER LNwURb GROUP BY name_yBRgesudEDnq,name_yBRgesudEDnq,carmake_yBRgesudEDnq,\"color_yBRgesudEDnq\",carmake_yBRgesudEDnq,carmake_yBRgesudEDnq,\"tag_yBRgesudEDnq\" ORDER BY color_yBRgesudEDnq", // /media/truecrypt1/src/kernel/src/expr.cpp:347: AfyRC::RC AfyKernel::ExprCompileCtx::compileValue(const Afy::Value&, ulong): Assertion `v.refV.flags!=0xFFFF' failed.
	NULL
};
char const * const TestMeanSql::sKeywords[] =
{
	"SELECT", "*", "FROM", "WHERE", "ORDER", "BY", "GROUP", "HAVING", "JOIN", "ON", "LEFT", "OUTER", 
	"RIGHT", "AND", "OR", "NOT", "UNION", "EXCEPT", "INTERSECT", "ALL", "ANY", "SOME", "MIN", "MAX",
	"ASC", "DISTINCT", "VALUES", "TRUE", "FALSE", "NULL", "AS", "DESC", "CURRENT_TIMESTAMP",
	"CURRENT_USER", "CURRENT_STORE", "BETWEEN", "COUNT", "IS", "TIMESTAMP", "INTERVAL", 
	"AGAINST", "MATCH", "UPPER", "LOWER", "NULLS", "FIRST", "LAST", "CROSS", "INNER", "USING",
	"WITH", "IDS", "INSERT", "UPDATE", "DELETE", "CREATE", "DROP", "PURGE", "UNDELETE",
	"SET", "ADD", "MOVE", "RENAME", "EDIT", "START", "TRANSACTION", "READ", "ONLY", "WRITE",
	"COMMIT", "ROLLBACK", "ISOLATION", "PART", "CLASS", "OPEN", "STORE", "BEFORE", "AFTER",
	"OPTIONS", "CLOSE", "TO", "BASE", "PREFIX", "CONSTRUCT", "DESCRIBE", "ASK", "GRAPH",
	"FILTER", "OPTIONAL", "REDUCED", "NAMED", "BOUND", "STR", "LANG", "LANGMATCHES",
	"DATATYPE", "ISURI", "ISIRI", "ISLITERAL", "SAMETERM", "A", ".", "!", "@", ":", ".", ":",
	NULL
};
char const * const TestMeanSql::sInfixOperators[] =
{
	"+", "-", "*", "/", "%", "&", "|", "^", "<<", ">>", "min", "max", "||",
	NULL
};
char const * const TestMeanSql::sInfixComparators[] =
{
	"<", ">", "<=", ">=", "=", "<>",
	NULL
};
char const * const TestMeanSql::sRadixProp[] =
{
	"name", "age", "profession", "friend"/*ref|fk*/, "tag"/*k*/, "owner"/*fk*/, "car"/*ref*/, "carmake", "color", "year"
};
char const * const TestMeanSql::sRadixClass[] =
{
	"people", "cars", "tags"
};

// Implement this test.
int TestMeanSql::execute()
{
	bool lSuccess = true;
	if (MVTApp::startStore())
	{
		ISession * const lSession = MVTApp::startSession();

		Tstring lSuffix;
		MVTRand::getString(lSuffix, 10, 10, false, true);
		size_t i, j;
		char lStmt[4096];

		mNumKnownFailures = countArray(sKnownFailures);
		mNumKeywords = countArray(sKeywords);
		mNumInfixOperators = countArray(sInfixOperators);
		mNumInfixComparators = countArray(sInfixComparators);

		for (i = 0; i < kpTOTAL; i++)
			mProperties[i] = Tstring(sRadixProp[i]) + "_" + lSuffix;
		for (i = 0; i < kcTOTAL; i++)
			mClasses[i] = Tstring(sRadixClass[i]) + "_" + lSuffix;

		mNumWords = mNumKeywords + kpTOTAL + kcTOTAL;
		mAllWords = new char const*[mNumWords];
		for (i = 0, j = 0; i < mNumKeywords; i++, j++)
			mAllWords[j] = sKeywords[i];
		for (i = 0; i < kpTOTAL; i++, j++)
			mAllWords[j] = mProperties[i].c_str();
		for (i = 0; i < kcTOTAL; i++, j++)
			mAllWords[j] = mClasses[i].c_str();

		// Create coherent classes.
		#if TEST_PRECREATE_GOOD_CLASSES
			mLogger.out() << std::endl << "***" << std::endl << "*** Creating coherent classes..." << std::endl << "***" << std::endl;
			sprintf(lStmt, "CREATE CLASS %s AS SELECT * WHERE EXISTS(%s);", mClasses[kcPeople].c_str(), mProperties[kpName].c_str());
			MVTApp::execStmt(lSession, lStmt);
			sprintf(lStmt, "CREATE CLASS %s AS SELECT * WHERE EXISTS(%s);", mClasses[kcCars].c_str(), mProperties[kpCarmake].c_str());
			MVTApp::execStmt(lSession, lStmt);
			sprintf(lStmt, "CREATE CLASS %s AS SELECT * WHERE %s IN :0;", mClasses[kcTags].c_str(), mProperties[kpTag].c_str());
			MVTApp::execStmt(lSession, lStmt);
		#endif

		// Create some coherent data.
		unsigned const lNumProfessions = 10;
		unsigned const lNumCarmakes = 10;
		unsigned const lNumColors = 10;
		#if TEST_PRECREATE_GOOD_DATA
			mLogger.out() << std::endl << "***" << std::endl << "*** Creating good data..." << std::endl << "***" << std::endl;
			// Insert some people (e.g. name=name12, age=27, profession=profession7).
			for (i = 0; i < 100; i++)
			{
				sprintf(
					lStmt, "INSERT %s=%s_%d, %s=%d, %s=%s_%d;",
					mProperties[kpName].c_str(), sRadixProp[kpName], i,
					mProperties[kpAge].c_str(), MVTRand::getRange(1, 100),
					mProperties[kpProfession].c_str(), sRadixProp[kpProfession], MVTRand::getRange(0, lNumProfessions - 1));
				MVTApp::execStmt(lSession, lStmt);
			}
			// Insert some cars (e.g. carmake=carmake3, year=1960, color=color7).
			for (i = 0; i < 50; i++)
			{
				sprintf(
					lStmt, "INSERT %s=%s_%d, %s=%d, %s=%s_%d;",
					mProperties[kpCarmake].c_str(), sRadixProp[kpCarmake], MVTRand::getRange(0, lNumCarmakes - 1),
					mProperties[kpYear].c_str(), 1950 + MVTRand::getRange(1, 60),
					mProperties[kpColor].c_str(), sRadixProp[kpColor], MVTRand::getRange(0, lNumColors - 1));
				MVTApp::execStmt(lSession, lStmt);
			}
			// TODO: Insert relationships (refs and foreign keys).
		#endif

		// Create some incoherent data.
		#if TEST_PRECREATE_INCOHERENT_DATA
			mLogger.out() << std::endl << "***" << std::endl << "*** Creating incoherent data..." << std::endl << "***" << std::endl;
		#endif

		// Meaningless random arrangements of keywords and keynames.
		#if TEST_MEANINGLESS_FULL
			mLogger.out() << std::endl << "***" << std::endl << "*** Testing meaningless random arrangements of 1-5 words..." << std::endl << "***" << std::endl;
			for (i = 0; i < 1000; i++)
				testRandomStatement(*lSession, 1, 5); // tiny sentences.
			mLogger.out() << std::endl << "***" << std::endl << "*** Testing meaningless random arrangements of 5-10 words..." << std::endl << "***" << std::endl;
			for (i = 0; i < 1000; i++)
				testRandomStatement(*lSession, 5, 10); // bigger sentences.
			mLogger.out() << std::endl << "***" << std::endl << "*** Testing meaningless random arrangements of 10-30 words..." << std::endl << "***" << std::endl;
			for (i = 0; i < 1000; i++)
				testRandomStatement(*lSession, 10, 30); // yet bigger sentences.
		#endif

		// Meaningless random arrangements of meaningful phrase fragments (e.g. ORDER BY keyname; ... AS ... JOIN ... AS ... ON ...; etc.).
		Tstring lTstStmt;
		#if TEST_MEANINGLESS_PARTIAL_A
			// Flavor 1: 85% of elements should be good, but no upper bound on number of bad elements.
			mLogger.out() << std::endl << "***" << std::endl << "*** Testing arrangements semi-meaningful fragments (85% | infinity)..." << std::endl << "***" << std::endl;
			GenCtx lGCtx1(0.85, false);
			for (i = 0; i < 10000; i++)
				testStmt(lSession, gen_statement(lGCtx1, lTstStmt).c_str());
		#endif
		#if TEST_MEANINGLESS_PARTIAL_B
			// Flavor 2: 75% of decisions should be good, and don't generate more than 1 bad thing per query.
			mLogger.out() << std::endl << "***" << std::endl << "*** Testing arrangements semi-meaningful fragments (75% | 1)..." << std::endl << "***" << std::endl;
			GenCtx lGCtx2(0.75, true, 1);
			for (i = 0; i < 10000; i++)
				testStmt(lSession, gen_statement(lGCtx2, lTstStmt).c_str());
		#endif
		#if TEST_MEANINGLESS_PARTIAL_C
			// Flavor 3: 40% of decisions should be good (almost unrecognizable sql?).
			mLogger.out() << std::endl << "***" << std::endl << "*** Testing arrangements semi-meaningful fragments (40% | infinity)..." << std::endl << "***" << std::endl;
			GenCtx lGCtx3(0.45, true);
			for (i = 0; i < 10000; i++)
				testStmt(lSession, gen_statement(lGCtx3, lTstStmt).c_str());
		#endif

		lSession->terminate();
		MVTApp::stopStore();
	}
	else { TVERIFY(!"could not open store") ; }
	return lSuccess ? 0 : 1;
}

// Simplest sub-test (meaningless random arrangements of keywords and keynames).
void TestMeanSql::testRandomStatement(ISession & pSession, size_t pMinWords, size_t pMaxWords)
{
	size_t i;
	size_t const lNumWords = MVTRand::getRange(pMinWords, pMaxWords);
	Tstring lStmt;
	for (i = 0; i < lNumWords; i++)
	{
		lStmt += MVTRand::getRange(0, 100) > 95 ? "@50001" : randomWord();
		lStmt += " ";
	}
	testStmt(&pSession, lStmt.c_str());
}

// Generators.
// Each generator tries to respect the 'spec' in pCtx, i.e. not generate more bad stuff than requested.
Tstring & TestMeanSql::gen_matching(GenCtx & pCtx, Tstring & pResult, char pOpening)
{
	if (pCtx.doGood())
	{
		switch (pOpening)
		{
			case '(': return pResult = ")";
			case '{': return pResult = "}";
			case '[': return pResult = "]";
			default: throw "Unexpected in gen_matching";
		}
	}
	static char const * sClosing[] = {")", "}", "]", "))", "}}", "]]", "(", "{", "[", ")(", "][", "}{", " "};
	return pResult = sClosing[MVTRand::getRange(0, sizeof(sClosing) / sizeof(sClosing[0]) - 1)];
}
Tstring & TestMeanSql::gen_label(GenCtx & pCtx, Tstring & pResult)
{
	Tstring lV;
	int const lType = pCtx.doGood() ? 0 : MVTRand::getRange(0, 5);
	switch (lType)
	{
		case 0: default: return pResult = MVTRand::getString(lV, 2, 5, false, true);
		case 1: return pResult = randomKeyword();
		case 2: return pResult = randomWord();
		case 3: return gen_value(pCtx, pResult);
	}
}
Tstring & TestMeanSql::gen_value(GenCtx & pCtx, Tstring & pResult)
{
	size_t i;
	Tstring lV;
	std::ostringstream lOs;
	// Either generate a good value...
	if (pCtx.doGood())
	{
		int const lType = MVTRand::getRange(0, 15);
		switch (lType)
		{
			// Note: We don't generate collections here, because it's not (yet) a recursive structure, and here we're supposed to be 'good'.
			case 0: default: MVTRand::getString(lV, 5, 15, true, true); lOs << "'" << lV << "'"; break;
			case 1: lOs << MVTRand::getRange(0, std::numeric_limits<int>::max()); if (MVTRand::getBool()) lOs << "U"; break;
			case 2: lOs << MVTRand::getFloatRange(0.0, std::numeric_limits<float>::max()) << "f"; break;
			case 3: lOs << MVTRand::getDoubleRange(0.0, std::numeric_limits<double>::max()); break;
			case 4: lOs << (MVTRand::getBool() ? "TRUE" : "FALSE"); break;
			case 5: MVTRand::getString(lV, 5, 15, true, true); lOs << "U'http://" << lV << "'"; break;
			case 6: MVTRand::getString(lV, 5, 15, true, true); lOs << "X'"; for (i = 0; i < lV.length(); i++) { lOs << std::hex << std::setw(2) << std::setfill('0') << (int)lV.at(i); } lOs << std::dec << "'"; break;
			case 7: lOs << "INTERVAL'" << MVTRand::getRange(0, std::numeric_limits<int>::max()) << ":" << std::setw(2) << std::setfill('0') << MVTRand::getRange(0, 59) << ":" << MVTRand::getRange(0, 59) << ".12345'"; break;
			case 8: lOs << "TIMESTAMP'" << std::setw(4) << std::setfill('0') << MVTRand::getRange(0, 2200) << "-" << std::setw(2) << MVTRand::getRange(1, 12) << "-" << MVTRand::getRange(1, 31) << " " << MVTRand::getRange(0, 23) << ":" << MVTRand::getRange(0, 59) << ":" << MVTRand::getRange(0, 59); break;
			case 9: lOs << "@" << std::hex << MVTRand::getRange(0x50001, 0x500001) << std::dec; break;
			case 10: lOs << "@" << std::hex << MVTRand::getRange(0x50001, 0x500001) << "." << randomProperty() << std::dec; break;
			case 11: lOs << "@" << std::hex << MVTRand::getRange(0x50001, 0x500001) << "." << randomProperty() << std::dec << "["; if (MVTRand::getBool()) lOs << ":FIRST"; else lOs << MVTRand::getRange(0, 100); lOs << "]"; break;
			case 12: lOs << gen_SELECT(pCtx, lV); break; // Not necessarily 'good' currently, but who cares...
			case 13: lOs << "${" << gen_SELECT(pCtx, lV) << "}"; break;
		}
	}
	// Or generate a bad value...
	else
	{
		int const lType = MVTRand::getRange(0, 18);
		switch (lType)
		{
			// Note: We do generate collections here, because here all is permitted :).
			case 0: default:
				#if TEST_ISSUE_DOUBLE_QUOTED_VAL
					MVTRand::getString(lV, 5, 15, true, true); lOs << "\"" << lV << "\""; break;
				#endif
			case 1: lOs << MVTRand::getRange(0, std::numeric_limits<int>::max()) << "bla"; break;
			case 2: lOs << MVTRand::getRange(0, std::numeric_limits<int>::max()) << MVTRand::getRange(0, std::numeric_limits<int>::max()) << MVTRand::getRange(0, std::numeric_limits<int>::max()); break;
			case 3: lOs << MVTRand::getFloatRange(0.0, std::numeric_limits<float>::max()) << "fbla"; break;
			case 4: lOs << MVTRand::getDoubleRange(0.0, std::numeric_limits<double>::max()) << "bla"; break;
			case 5: lOs << MVTRand::getDoubleRange(0.0, std::numeric_limits<double>::max()) << MVTRand::getDoubleRange(0.0, std::numeric_limits<double>::max()); break;
			case 6: lOs << randomKeyword(); break;
			case 7: lOs << gen_collection(pCtx, lV); break;
			case 8: lOs << "MAYBE"; break;
			case 9: MVTRand::getString(lV, 5, 15, true, true); lOs << "U'http://" << lV << (pCtx.doGood() ? "'" : ""); break;
			case 10: MVTRand::getString(lV, 5, 15, true, true); lOs << "X'"; for (i = 0; i < lV.length(); i++) { lOs << std::hex << (int)lV.at(i); } lOs << std::dec << (pCtx.doGood() ? "'" : ""); break;
			case 11: lOs << "INTERVAL'"; if (pCtx.doGood()) lOs << MVTRand::getRange(0, std::numeric_limits<int>::max()); else lOs << MVTRand::getString(lV, 5, 15, false, true); lOs << ":" << MVTRand::getRange(0, pCtx.doGood() ? 59 : 1000) << (pCtx.doGood() ? ":" : ";") << MVTRand::getRange(0, pCtx.doGood() ? 59 : 1000) << ".12345'"; break;
			case 12: lOs << "TIMESTAMP'" << MVTRand::getRange(0, 10000) << "-" << MVTRand::getRange(1, pCtx.doGood() ? 12 : 1000) << "-" << MVTRand::getRange(1, pCtx.doGood() ? 31 : 1000) << " " << MVTRand::getRange(0, pCtx.doGood() ? 23 : 1000) << ":" << MVTRand::getRange(0, 59) << ":" << MVTRand::getRange(0, 59); break;
			case 13: lOs << "@"; if (pCtx.doGood()) lOs << std::hex << MVTRand::getRange(0x50001, 0x500001) << std::dec; else lOs << MVTRand::getString(lV, 5, 15, false, true); break;
			case 14: lOs << "@" << std::hex << MVTRand::getRange(0x50001, 0x500001) << "." << (pCtx.doGood() ? randomProperty() : randomWord()) << std::dec; break;
			case 15: lOs << "@" << std::hex << MVTRand::getRange(0x50001, 0x500001) << "." << randomProperty() << std::dec << "["; if (pCtx.doGood()) lOs << MVTRand::getRange(0, 100); else lOs << randomWord(); lOs << "]"; break;
			case 16: lOs << "${" << gen_whatever(pCtx, lV) << "}"; break;
		}
	}
	return pResult = lOs.str();
}
Tstring & TestMeanSql::gen_collection(GenCtx & pCtx, Tstring & pResult)
{
	Tstring lV;
	std::ostringstream lOs;
	bool const lDoGood = pCtx.doGood();
	size_t const lNumVals = MVTRand::getRange(1, pCtx.mShort ? 3 : 10);
	lOs << "{";
	if (lDoGood)
	{
		for (size_t i = 0; i < lNumVals; i++)
		{
			lOs << gen_value(pCtx, lV);
			if (i + 1 != lNumVals)
				lOs << ",";
		}
		lOs << "}";
	}
	else
	{
		for (size_t i = 0; i < lNumVals; i++)
		{
			lOs << (pCtx.doGood() ? gen_value(pCtx, lV) : gen_whatever(pCtx, lV));
			if (i + 1 != lNumVals || !pCtx.doGood())
				lOs << ",";
		}
		lOs << gen_matching(pCtx, lV, '{');
	}
	return pResult = lOs.str();
}
Tstring & TestMeanSql::gen_assignment(GenCtx & pCtx, Tstring & pResult)
{
	// Either generate good assignments, or bad assignments.
	Tstring lV;
	std::ostringstream lOs;
	bool const lDoGood = pCtx.doGood();
	int const lTypeLHS = lDoGood ? 0 : MVTRand::getRange(0, 5);
	switch (lTypeLHS)
	{
		case 0: default: lOs << randomProperty(); break;
		case 1: lOs << randomClass(); break;
		case 2: lOs << randomKeyword(); break;
		case 3: lOs << gen_SELECT(pCtx, lV); break;
		case 4: lOs << gen_whatever(pCtx, lV); break;
	}
	int const lTypeOp = lDoGood ? 0 : MVTRand::getRange(0, 5);
	switch (lTypeOp)
	{
		case 0: default: lOs << "="; break;
		case 1: lOs << randomInfixOperator(); break;
		case 2: lOs << randomInfixOperator() << "="; break;
		case 3: lOs << randomInfixOperator() << randomInfixOperator(); break;
	}
	int const lTypeRHS = lDoGood ? 0 : MVTRand::getRange(0, 3);
	switch (lTypeRHS)
	{
		case 0: default: lOs << (MVTRand::getBool() ? gen_value(pCtx, lV) : gen_collection(pCtx, lV)); break;
		case 1: lOs << gen_whatever(pCtx, lV); break;
	}
	return pResult = lOs.str();
}
Tstring & TestMeanSql::gen_projection(GenCtx & pCtx, Tstring & pResult, bool pWithAs)
{
	// Either generate good projections, or bad projections.
	Tstring lV;
	std::ostringstream lOs;
	size_t const lNumProps = MVTRand::getRange(1, pCtx.mShort ? 3 : 10);
	bool const lDoGood = pCtx.doGood();
	for (size_t i = 0; i < lNumProps; i++)
	{
		int const lType = lDoGood ? 0 : MVTRand::getRange(0, 5);
		switch (lType)
		{
			case 0: default: if (MVTRand::getBool()) { lOs << randomProperty(); } else { lOs << "\"" << randomProperty() << "\""; } break;
			case 1: lOs << randomKeyword(); break;
			case 2: lOs << randomClass(); break;
			case 3: lOs << "*"; break; // Note: included in randomKeyword, but giving it a little probabilistic boost...
			case 4: lOs << gen_whatever(pCtx, lV); break;
		}
		if (pWithAs && MVTRand::getBool())
		{
			lOs << " " << (lDoGood ? "AS" : randomKeyword()) << " ";
			lOs << (lDoGood ? MVTRand::getString(lV, 5, 15, false, true) : gen_label(pCtx, lV));
		}
		if (i + 1 != lNumProps || (!lDoGood && !pCtx.doGood()))
			lOs << ",";
	}
	pCtx.mProduced = lNumProps;
	return pResult = lOs.str();
}
Tstring & TestMeanSql::gen_INSERT_newstyle(GenCtx & pCtx, Tstring & pResult)
{
	Tstring lV;
	std::ostringstream lOs;
	size_t const lNumProps = MVTRand::getRange(1, pCtx.mShort ? 3 : 10);
	lOs << "INSERT ";
	if (pCtx.doGood())
	{
		for (size_t i = 0; i < lNumProps; i++)
		{
			lOs << gen_assignment(pCtx, lV);
			if (i + 1 != lNumProps)
				lOs << ",";
		}
	}
	else
	{
		for (size_t i = 0; i < lNumProps; i++)
		{
			lOs << (pCtx.doGood() ? gen_assignment(pCtx, lV) : gen_whatever(pCtx, lV));
			if (i + 1 != lNumProps || !pCtx.doGood())
				lOs << ",";
		}
	}
	return pResult = lOs.str();
}
Tstring & TestMeanSql::gen_INSERT_oldstyle(GenCtx & pCtx, Tstring & pResult)
{
	Tstring lV;
	std::ostringstream lOs;
	lOs << "INSERT ";
	if (pCtx.doGood())
	{
		lOs << "(" << gen_projection(pCtx, lV, false) << ") VALUES (";
		size_t const lNumProps = pCtx.mProduced;
		for (size_t i = 0; i < lNumProps; i++)
		{
			lOs << (MVTRand::getBool() ? gen_value(pCtx, lV) : gen_collection(pCtx, lV));
			if (i + 1 != lNumProps)
				lOs << ",";
		}
		lOs << ")";
	}
	else
	{
		lOs << "(" << gen_projection(pCtx, lV, pCtx.doGood());
		size_t const lNumProps = pCtx.mProduced + MVTRand::getRange(0, 5);
		lOs << gen_matching(pCtx, lV, '(') << " ";
		lOs << (pCtx.doGood() ? "VALUES" : randomWord().c_str()) << " ";
		lOs << "(";
		for (size_t i = 0; i < lNumProps; i++)
		{
			lOs << (pCtx.doGood() ? (MVTRand::getBool() ? gen_value(pCtx, lV) : gen_collection(pCtx, lV)) : gen_whatever(pCtx, lV));
			if (i + 1 != lNumProps || !pCtx.doGood())
				lOs << ",";
		}
		lOs << gen_matching(pCtx, lV, '(');
	}
	return pResult = lOs.str();
}
Tstring & TestMeanSql::gen_UPDATE(GenCtx & pCtx, Tstring & pResult)
{
	// TODO
	// get a pin; or update based on a condition
	// set vs add
	return pResult;
}
Tstring & TestMeanSql::gen_DELETE(GenCtx & pCtx, Tstring & pResult)
{
	// TODO
	// pin vs cond/class
	// delete vs purge vs undelete
	return pResult;
}
Tstring & TestMeanSql::gen_CLASS(GenCtx & pCtx, Tstring & pResult)
{
	// TODO
	// any select vs ...
	// class vs family
	// multi-seg
	return pResult;
}
Tstring & TestMeanSql::gen_SELECT(GenCtx & pCtx, Tstring & pResult)
{
	Tstring lV, lJLeft, lJRight;
	std::ostringstream lOs;
	lOs << "SELECT ";
	bool const lWithProjection = MVTRand::getBool();
	bool const lWithFROM = MVTRand::getBool();
	bool const lWithJOIN = lWithFROM && MVTRand::getBool();
	bool const lWithWHERE = MVTRand::getBool();
	bool const lWithGROUPBY = MVTRand::getBool();
	bool const lWithHAVING = lWithGROUPBY && MVTRand::getBool();
	bool const lWithORDERBY = MVTRand::getBool();
	bool const lWithOTHER = MVTRand::getBool();
	lOs << (lWithProjection ? gen_projection(pCtx, lV, true) : "*");
	if (lWithFROM)
	{
		if (lWithJOIN)
		{
			gen_label(pCtx, lJLeft); gen_label(pCtx, lJRight);
			lOs << " FROM " << (pCtx.doGood() ? randomClass() : gen_whatever(pCtx, lV));
			lOs << " " << (pCtx.doGood() ? "AS" : randomKeyword()) << " " << lJLeft;
			lOs << " " << (pCtx.doGood() ? "JOIN" : randomKeyword()) << " " << (pCtx.doGood() ? randomClass() : gen_whatever(pCtx, lV));
			lOs << " " << (pCtx.doGood() ? "AS" : randomKeyword()) << " " << lJRight;
			lOs << " " << (pCtx.doGood() ? "ON" : randomKeyword()) << " " << (pCtx.doGood() ? "(" : "{");
			lOs << lJLeft << (pCtx.doGood() ? "." : "->") << (pCtx.doGood() ? randomProperty() : gen_whatever(pCtx, lV));
			lOs << (pCtx.doGood() ? "=" : randomInfixComparator());
			lOs << lJRight << (pCtx.doGood() ? "." : "->") << (pCtx.doGood() ? randomProperty() : gen_whatever(pCtx, lV));
			lOs << gen_matching(pCtx, lV, '(');
		}
		else
			lOs << " FROM " << (pCtx.doGood() ? randomClass() : gen_whatever(pCtx, lV));
	}
	if (lWithWHERE)
	{
		lOs << " WHERE " << (pCtx.doGood() ? "(" : "{");
		size_t const lNumConds = MVTRand::getRange(1, pCtx.mShort ? 3 : 6);
		for (size_t i = 0; i < lNumConds; i++)
		{
			if (lWithJOIN)
			{
				lOs << (pCtx.doGood() ? (MVTRand::getBool() ? lJLeft : lJRight) : randomWord());
				lOs << (pCtx.doGood() ? "." : "->");
			}
			lOs << (pCtx.doGood() ? randomProperty() : randomWord());
			lOs << randomInfixComparator();
			lOs << gen_value(pCtx, lV);
			if (i + 1 != lNumConds)
				lOs << (MVTRand::getBool() ? " AND " : " OR ");
		}
		lOs << gen_matching(pCtx, lV, '(');
	}
	if (lWithGROUPBY)
	{
		lOs << " GROUP " << (pCtx.doGood() ? "BY" : randomWord()) << " ";
		lOs << gen_projection(pCtx, lV, false);
		// TODO: lWithHAVING
	}
	if (lWithORDERBY)
	{
		lOs << " ORDER " << (pCtx.doGood() ? "BY" : randomWord()) << " ";
		lOs << (pCtx.doGood() ? randomProperty() : gen_whatever(pCtx, lV));
	}
	if (lWithOTHER)
	{
		// TODO
	}
	return pResult = lOs.str();
}
Tstring & TestMeanSql::gen_statement(GenCtx & pCtx, Tstring & pResult)
{
	int const lType = MVTRand::getRange(0, 2/*XXX*/);
	switch (lType)
	{
		default:
		#if TEST_INSERT
		case 0: return gen_INSERT_newstyle(pCtx, pResult);
		case 1: return gen_INSERT_oldstyle(pCtx, pResult);
		#endif
		#if TEST_SELECT
		case 2: return gen_SELECT(pCtx, pResult);
		#endif
		#if TEST_UPDATE
		case 3: return gen_UPDATE(pCtx, pResult);
		#endif
		#if TEST_DELETE
		case 4: return gen_DELETE(pCtx, pResult);
		#endif
		case 5: return gen_CLASS(pCtx, pResult);
	}
	return pResult = "";
}
Tstring & TestMeanSql::gen_whatever(GenCtx & pCtx, Tstring & pResult)
{
	int const lType = MVTRand::getRange(0, 10);
	switch (lType)
	{
		case 0: default: return gen_value(pCtx, pResult);
		case 1: return gen_collection(pCtx, pResult);
		case 2: return gen_assignment(pCtx, pResult);
		case 3: return gen_projection(pCtx, pResult, MVTRand::getBool());
		case 4: return gen_statement(pCtx, pResult);
		case 5: return pResult = randomWord();
		case 6: return pResult = randomInfixOperator();
		case 7: return pResult = randomInfixComparator();
	}
	return pResult = "";
}
