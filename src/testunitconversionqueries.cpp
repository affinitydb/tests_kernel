/**************************************************************************************

Copyright © 2004-2013 GoPivotal, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"


class testunitconversionqueries : public ITest
{
public:
	TEST_DECLARE(testunitconversionqueries);
	virtual char const * getName() const { return "testunitconversionqueries"; }
	virtual char const * getHelp() const { return ""; }
	virtual char const * getDescription() const { return "Test to verify the unit conversion in queries"; }
	virtual int execute();
	virtual void destroy() { delete this; }

public:
	ISession * mySession ;
	PropertyID	myProp[30] ;
	SourceSpec lCS;
	
	struct TestUnits
	{
		Units			pu;
		const char		*UnitName;
		int				UnitType;
		double			UnitSize;
	};

protected:
	void createUnitPins();
	void verifyMulDiv(double d1,Units u1,ExprOp op,double d2,Units u2,Units expectedUnit);
};
TEST_IMPLEMENT(testunitconversionqueries, TestLogger::kDStdOut);

int testunitconversionqueries::execute()
{
	if (!MVTApp::startStore()) {mLogger.print("Failed to start store\n"); return RC_NOACCESS;}
		
	mySession = MVTApp::startSession();
	createUnitPins();

	verifyMulDiv(1,Un_m,OP_MUL,1,Un_m,Un_m2);			//meter square
	verifyMulDiv(1,Un_cm,OP_MUL,1,Un_cm,Un_cm2);		//centimeter square
	verifyMulDiv(1,Un_ft,OP_MUL,1,Un_ft,Un_sq_ft);	//square foot
	verifyMulDiv(1,Un_m,OP_DIV,1,Un_s,Un_mps);		//Meter per second
	verifyMulDiv(.001,Un_km,OP_DIV,1,Un_s,Un_mps);	//Meter per second 
	verifyMulDiv(1,Un_ft,OP_DIV,60,Un_s,Un_fpm);		//Feet per Minute
	verifyMulDiv(1,Un_mi,OP_DIV,3600,Un_s,Un_mph);	//Miles per hour
	verifyMulDiv(1,Un_km,OP_DIV,3600,Un_s,Un_kph);	//Kilometer per hour (Failing)
	verifyMulDiv(1,Un_m2,OP_MUL,1,Un_m,Un_m3);		//Meter cube
	verifyMulDiv(100,Un_cm2,OP_MUL,10,Un_cm,Un_L);	//Litre
	verifyMulDiv(1,Un_N,OP_DIV,1,Un_m2,Un_Pa);		//Pascal= Newton per meter square

	verifyMulDiv(1000,Un_m,OP_MUL,1,Un_NDIM,Un_km);	//1000 meters = 1 km
	verifyMulDiv(.00001,Un_b,OP_MUL,1,Un_NDIM,Un_Pa);//1 Pascal = .00001 bar

	mySession->terminate();
	MVTApp::stopStore();

	return RC_OK;
}

void testunitconversionqueries::createUnitPins(void)
{
	TestUnits units[] = 
		{
		{Un_m,					"meter",		1,  		1.},
		{Un_kg,				"kilogram",		2,  		1.},
		{Un_s,					"second",		3,  		1.},
		{Un_A,					"ampere",		4,  		1.},
		{Un_K,					"kelvin",		5,  		1.},
		{Un_mol,				"mole",			6,				1.},
		{Un_cd,				"candela",		7,  		1.},

		{Un_Hz,				"hertz",		8,  		1.},
		{Un_N,					"newton",		9,  		1.},
		{Un_Pa,				"pascal",		10,  		1.},
		{Un_J,					"joule",		11,  		1.},
		{Un_W,					"watt",			12,  		1.},
		{Un_C,					"coulomb",		13,  		1.},
		{Un_V,					"volt",			14,  		1.},
		{Un_F,					"farad",		15,  		1.},
		{Un_Ohm,				"ohm",			16,  		1.},
		{Un_S,					"siemens",		17,  		1.},
		{Un_Wb,				"weber",		18,  		1.},
		{Un_T,					"tesla",		19,  		1.},
		{Un_H,					"henri",		20,  		1.},
		{Un_dC,				"degree Celsius",	5,  		273.15},
		{Un_rad,				"radian",		21,  		1.},
		{Un_sr,				"steradian",	22,  		1.},
		{Un_lm,				"lumen",		23,  		1.},
		{Un_lx,				"lux",			24,  		1.},
		{Un_Bq,				"becquerel",	8,  		1.},
		{Un_Gy,				"gray",			25,  		1.},
		{Un_Sv,				"sievert",		25,  		1.},
		{Un_kat,				"katal",		26,  		1.},

		{Un_dm,				"dm",			1,  		1.E-1},
		{Un_cm,				"centimeter",	1,  		1.E-2},
		{Un_mm,				"mm",			1,  		1.E-3},
		{Un_mkm,				"mkm",			1,  		1.E-6},
		{Un_nm,				"nm",			1,  		1.E-9},
		{Un_km,				"km",			1,  		1.E+3},
		{Un_in,				"inch",			1,  		0.0254},
		{Un_ft,				"foot",			1,  		0.3048},
		{Un_yd,				"yard",			1,  		0.9144},
		{Un_mi,				"mile",			1,  		1609.344},
		{Un_nmi,				"nautical mile",1,  		1852.},

		{Un_au,				"astronomical unit",1,  		149597870691.},
		{Un_pc,				"parsec",		1,  		30.85678E+15},
		{Un_ly,				"light year",	1,  		9.460730473E+15},

		{Un_mps,				"meters per second",	27,  		1.},
		{Un_kph,				"kilometers per hour",	27,  		0.277777777777777777777777777},
		{Un_fpm,				"feet per minute",		27,  		0.00508},
		{Un_mph,				"miles per hour",		27,  		0.44704},
		{Un_kt,				"knot",					27,  		0.514444444444444444444444444},

		{Un_g,					"gram",					2,  		1.E-3},
		{Un_mg,				"milligram",		2,  		1.E-6},
		{Un_mkg,				"microgram",		2,  		1.E-9},
		{Un_t,					"ton",		2,  		1000.},

		{Un_lb,				"pound",	2,  		0.45359237},
		{Un_oz,				"ounce",	2,  		0.0283495231},
		{Un_st,				"stone",	2,  		6.35029},

		{Un_m2,				"square meter",	28,  		1.},
		{Un_cm2,				"square centimeter",	28,  		1.E-4},
		{Un_sq_in,				"square inch",28,  		6.4516E-4},
		{Un_sq_ft,				"square foot",28,  		0.09290304},
		{Un_ac,				"acre",		28,  		4046.873},
		{Un_ha,				"hectare",	28,  		10000.},

		{Un_m3,				"cubic meter",	29,  		1.},
		{Un_L,					"liter",		29,  		1.E-3},
		{Un_cl,				"centiliter",	29,  		1.E-5},
		{Un_cm3,				"cubic centimeter",	29,  		1.E-6},
		{Un_cf,				"cubic foot",	29,  		0.02831685},
		{Un_ci,				"cubic inch",	29,  		1.63871E-5},
		{Un_fl_oz,				"fluid once",	29,  		29.573531E-6},

		{Un_bbl,				"barrel",	29,  		0.158987},
		{Un_bu,				"bushel",	29,  		3.523907E-2},
		{Un_gal,				"gallon",	29,  		3.785411784E-3},
		{Un_qt,				"quart",	29,  		0.946352946E-3},
		{Un_pt,				"pint",		29,  		0.473176473E-3},

		{Un_b,					"bar",					10,  		1.E+5},
		{Un_mmHg,				"millimeters of mercury",10,  	133.3},
		{Un_inHg,				"inches of mercury",	10,  		3.38638E+3},

		{Un_cal,				"calorie",				11,  		4.1868},
		{Un_kcal,				"Calorie",				11,  		4.1868E+3},
		{Un_ct,				"carat",				2,  		2.E-4},
		{Un_dF,				"degrees Fahrenheit",	5,  		2.5559277776},
	};
	int i;
	char *propName1="class.unitsAll";
	char *propName2;
	double d=1;
	
	myProp[0]=MVTUtil::getProp(mySession,propName1);  //Common property added to all pins. Class defined with this property to avoid full scan.

	for(i=1;i<30;i++)		//Creating propety for each unit type.
	{
		char lB[64]; sprintf(lB, "%d.units.prop", i);
		propName2=lB;
		myProp[i]=MVTUtil::getPropRand(mySession,propName2);
	}

	for(i=0;i<Un_ALL-1;i++)   //Creating one pin each for every measurement units.
	{
		Value testvalue[2];
		testvalue[0].set("unit class");
		testvalue[0].property=myProp[0];
		testvalue[1].set(d,units[i].pu);
		testvalue[1].property=myProp[units[i].UnitType];

		TVERIFYRC(mySession->createPIN(testvalue,2,NULL,MODE_COPY_VALUES|MODE_PERSISTENT));
	}
	
	/* creating class and classSpec*/
	ClassID CLSID = STORE_INVALID_CLASSID;
	IStmt * pAllPINsWithUnit = mySession->createStmt() ;
	unsigned char v = pAllPINsWithUnit->addVariable() ;
	pAllPINsWithUnit->setPropCondition( v, &myProp[0], 1 ) ;
	defineClass(mySession, propName1, pAllPINsWithUnit, &CLSID ) ;
	pAllPINsWithUnit->destroy();

	lCS.objectID = CLSID;
	lCS.nParams = 0;
	lCS.params = NULL;

	for(i=0;i<Un_ALL-1;i++)  //Querying for pins of each measurement units.
	{
		IStmt * const lQ =mySession->createStmt();
		unsigned const lVar= lQ->addVariable(&lCS,1);
		Value lV[2];
		uint64_t lCount = 0;
		
		lV[0].setVarRef(0,myProp[units[i].UnitType]);
		lV[1].set(d,units[i].pu);
		IExprNode * const lE = mySession->expr(OP_EQ, 2, lV);
		lQ->addCondition(lVar,lE);
		lQ->count(lCount);
		if(i==7||i==25||i==26||i==27) //These are different units with same convesrion ratio.
			TVERIFY(lCount==2); 
		else
			TVERIFY(lCount==1); //Only 1 pin should exist for each measurement units.
		lQ->destroy();
	} //End of For loop
}

void testunitconversionqueries::verifyMulDiv(double d1,Units u1,ExprOp op,double d2,Units u2,Units expectedUnit)
{	
	IStmt * const lQ1 =mySession->createStmt();
	ICursor *myResult;
	IPIN *pin;
	IExprNode * lE1;
	unsigned lVar1;
	Value lV[2];
	const Value * rV;
	uint64_t lCount = 0;	
	TestUnits units[] = 
	{
		{Un_m,					"meter",		1,  		1.},
		{Un_kg,				"kilogram",		2,  		1.},
		{Un_s,					"second",		3,  		1.},
		{Un_A,					"ampere",		4,  		1.},
		{Un_K,					"kelvin",		5,  		1.},
		{Un_mol,				"mole",			6,				1.},
		{Un_cd,				"candela",		7,  		1.},

		{Un_Hz,				"hertz",		8,  		1.},
		{Un_N,					"newton",		9,  		1.},
		{Un_Pa,				"pascal",		10,  		1.},
		{Un_J,					"joule",		11,  		1.},
		{Un_W,					"watt",			12,  		1.},
		{Un_C,					"coulomb",		13,  		1.},
		{Un_V,					"volt",			14,  		1.},
		{Un_F,					"farad",		15,  		1.},
		{Un_Ohm,				"ohm",			16,  		1.},
		{Un_S,					"siemens",		17,  		1.},
		{Un_Wb,				"weber",		18,  		1.},
		{Un_T,					"tesla",		19,  		1.},
		{Un_H,					"henri",		20,  		1.},
		{Un_dC,				"degree Celsius",	5,  		273.15},
		{Un_rad,				"radian",		21,  		1.},
		{Un_sr,				"steradian",	22,  		1.},
		{Un_lm,				"lumen",		23,  		1.},
		{Un_lx,				"lux",			24,  		1.},
		{Un_Bq,				"becquerel",	8,  		1.},
		{Un_Gy,				"gray",			25,  		1.},
		{Un_Sv,				"sievert",		25,  		1.},
		{Un_kat,				"katal",		26,  		1.},

		{Un_dm,				"dm",			1,  		1.E-1},
		{Un_cm,				"centimeter",	1,  		1.E-2},
		{Un_mm,				"mm",			1,  		1.E-3},
		{Un_mkm,				"mkm",			1,  		1.E-6},
		{Un_nm,				"nm",			1,  		1.E-9},
		{Un_km,				"km",			1,  		1.E+3},
		{Un_in,				"inch",			1,  		0.0254},
		{Un_ft,				"foot",			1,  		0.3048},
		{Un_yd,				"yard",			1,  		0.9144},
		{Un_mi,				"mile",			1,  		1609.344},
		{Un_nmi,				"nautical mile",1,  		1852.},

		{Un_au,				"astronomical unit",1,  		149597870691.},
		{Un_pc,				"parsec",		1,  		30.85678E+15},
		{Un_ly,				"light year",	1,  		9.460730473E+15},

		{Un_mps,				"meters per second",	27,  		1.},
		{Un_kph,				"kilometers per hour",	27,  		0.2777777777778},
		{Un_fpm,				"feet per minute",		27,  		0.00508},
		{Un_mph,				"miles per hour",		27,  		0.44704},
		{Un_kt,				"knot",					27,  		0.5144444444444},

		{Un_g,					"gram",					2,  		1.E-3},
		{Un_mg,				"milligram",		2,  		1.E-6},
		{Un_mkg,				"microgram",		2,  		1.E-9},
		{Un_t,					"ton",		2,  		1000.},

		{Un_lb,				"pound",	2,  		0.45359237},
		{Un_oz,				"ounce",	2,  		0.0283495231},
		{Un_st,				"stone",	2,  		6.35029},

		{Un_m2,				"square meter",	28,  		1.},
		{Un_cm2,				"square centimeter",	28,  		1.E-4},
		{Un_sq_in,				"square inch",28,  		6.4516E-4},
		{Un_sq_ft,				"square foot",28,  		0.09290304},
		{Un_ac,				"acre",		28,  		4046.873},
		{Un_ha,				"hectare",	28,  		10000.},

		{Un_m3,				"cubic meter",	29,  		1.},
		{Un_L,					"liter",		29,  		1.E-3},
		{Un_cl,				"centiliter",	29,  		1.E-5},
		{Un_cm3,				"cubic centimeter",	29,  		1.E-6},
		{Un_cf,				"cubic foot",	29,  		0.02831685},
		{Un_ci,				"cubic inch",	29,  		1.63871E-5},
		{Un_fl_oz,				"fluid once",	29,  		29.573531E-6},

		{Un_bbl,				"barrel",	29,  		0.158987},
		{Un_bu,				"bushel",	29,  		3.523907E-2},
		{Un_gal,				"gallon",	29,  		3.785411784E-3},
		{Un_qt,				"quart",	29,  		0.946352946E-3},
		{Un_pt,				"pint",		29,  		0.473176473E-3},

		{Un_b,					"bar",					10,  		1.E+5},
		{Un_mmHg,				"millimeters of mercury",10,  	133.3},
		{Un_inHg,				"inches of mercury",	10,  		3.38638E+3},

		{Un_cal,				"calorie",				11,  		4.1868},
		{Un_kcal,				"Calorie",				11,  		4.1868E+3},
		{Un_ct,				"carat",				2,  		2.E-4},
		{Un_dF,				"degrees Fahrenheit",	5,  		2.5559277776},
	};
		
	lV[0].set(d1,u1);
	lV[1].set(d2,u2);
	lVar1 = lQ1->addVariable(&lCS,1);
	lE1 = mySession->expr(op, 2, lV);
	lV[0].set(lE1);
	lV[1].setVarRef(0,myProp[units[expectedUnit-1].UnitType]);
	IExprNode * const lE2 = mySession->expr(OP_EQ, 2, lV);
	lQ1->addCondition(lVar1,lE2);
	lQ1->count(lCount);
	TVERIFY(lCount==1);
	if(lCount!=1)
		mLogger.out()<<endl<<"verifyMulDiv failed for "<<units[expectedUnit-1].UnitName<<endl;

	TVERIFYRC(lQ1->execute(&myResult));
	while( pin = myResult->next() )
		{
		rV=pin->getValue(myProp[units[expectedUnit-1].UnitType]);
		TVERIFY(rV->qval.units==expectedUnit);
		}

		lE2->destroy();
	myResult->destroy();
	lQ1->destroy();
}
