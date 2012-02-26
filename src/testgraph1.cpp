/**************************************************************************************

Copyright © 2004-2011 VMware, Inc. All rights reserved.

**************************************************************************************/

#include "app.h"
#include <fstream>
#include <stdlib.h>
#include <map>
#include <set>
#include <stdio.h>
#include <string>
#include <vector>
using namespace AfyDB;
using namespace std;

typedef std::vector<std::string> TStrings;
typedef std::vector<IPIN *> TPINs;
typedef std::vector<uint32_t> TUint32s;
typedef std::set<PID> TPIDs;

#define PARAMS_TINY "50_10_50_100"
#define PARAMS_SMALL "2000_50_200_2000"
#define PARAMS_MEDIUM "10000_100_200_2000"
#define PARAMS_SERIES1_A "500_50_1000_3000"
#define PARAMS_SERIES1_B "5000_50_1000_3000" // "CENTRAL"
#define PARAMS_SERIES1_C "10000_50_1000_3000"
#define PARAMS_SERIES1_D "20000_50_1000_3000"
#define PARAMS_ACTUAL PARAMS_TINY
char const * sPeopleFN = "../people_"PARAMS_ACTUAL".txt";
char const * sProjectsFN = "../projects_"PARAMS_ACTUAL".txt";
char const * sPhotosFN = "../photos_"PARAMS_ACTUAL".txt";
char const * const sProps[] = {"orgid", "firstname", "middlename", "lastname", "occupation", "country", "postalcode", "friendof", "rootproject", "fid", "children", "access", "fname", "pname"};
enum eProps { P_ORGID = 0, P_FIRSTNAME, P_MIDDLENAME, P_LASTNAME, P_OCCUPATION, P_COUNTRY, P_POSTALCODE, P_FRIENDOF, P_ROOTPROJECT, P_FID, P_CHILDREN, P_ACCESS, P_FNAME, P_PNAME, P_TOTALCOUNT };
PropertyID sPropIDs[P_TOTALCOUNT];
ClassID sClsid_orgid = STORE_INVALID_CLASSID;
ClassID sClsid_fid = STORE_INVALID_CLASSID;
size_t const sTxSize = 1024;

long countLines(char const * pFN)
{
    long lLineNum = 0;
    std::ifstream lIs(pFN);
    char lLine[32768];
    while (lIs.is_open() && !lIs.eof())
    {
        lIs.getline(lLine, sizeof(lLine));
        lLineNum++;
    }
    return lLineNum;
}

void loadPeople(ISession & pSession, int pPass)
{
    long const lTotalLineNum = countLines(sPeopleFN);
    TPINs lPINs;
    std::ifstream lIs(sPeopleFN);
    char lLine[32768];
    long lLineNum = 0, lRefNum = 0;
    printf("%3d%%", 0); fflush(stdout);
    long const lT1 = getTimeInMs();
    if (2 == pPass)
        pSession.startTransaction();
    while (lIs.is_open() && !lIs.eof())
    {
        lIs.getline(lLine, sizeof(lLine));
        if (0 == strlen(lLine))
            continue;
        if (0 == lLineNum % 1000)
            { printf("\b\b\b\b\b%3d%%", int(1.0 + 100.0 * lLineNum / lTotalLineNum)); fflush(stdout); }

        // Parse the line.
        // (1 'Winnie' 'Anne' 'Jones' 'ad writer' 'Uruguay' 'B5G 3U6' (2 7 8 18 29 60 62 71 78 85))
        uint32_t lOrgId;
        if (1 != sscanf(&lLine[1], "%u", &lOrgId))
            printf("ERROR(%d): Couldn't read orgid.\n", __LINE__);
        TStrings lAttributes;
        int i;
        char * lQuote1 = strchr(lLine, '\'');
        char * lQuote2 = strchr(lQuote1 + 1, '\'');
        for (i = 0;;)
        {
            lAttributes.push_back(std::string(lQuote1 + 1, lQuote2 - lQuote1 - 1));
            i++;
            if (i >= 6)
                break;
            lQuote1 = strchr(lQuote2 + 1, '\'');
            lQuote2 = strchr(lQuote1 + 1, '\'');
        }
        char * lLp = strchr(lQuote2, '(');
        char * lRp = strchr(lLp, ')');
        std::string lAllRefs(lLp + 1, lRp - lLp - 1);

        TStrings lRefs;
        char * lTk = strtok((char *)lAllRefs.c_str(), " ");
        while (lTk != NULL)
        {
            lRefs.push_back(std::string(lTk));
            lTk = strtok(NULL, " ");
        }
        
        #if 0
            printf("firstname: %s\n", lAttributes[0].c_str());
            printf("middlename: %s\n", lAttributes[1].c_str());
            printf("lastname: %s\n", lAttributes[2].c_str());
            for (TStrings::iterator iS = lRefs.begin(); lRefs.end() != iS; iS++)
                printf("  ref: %s\n", (*iS).c_str());
            printf("\n");
        #endif

        switch (pPass)
        {
            case 1:
            {
                Value lV[7];
                SETVALUE(lV[0], sPropIDs[P_ORGID], lOrgId, OP_SET);
                SETVALUE(lV[1], sPropIDs[P_FIRSTNAME], lAttributes[0].c_str(), OP_SET);
                SETVALUE(lV[2], sPropIDs[P_MIDDLENAME], lAttributes[1].c_str(), OP_SET);
                SETVALUE(lV[3], sPropIDs[P_LASTNAME], lAttributes[2].c_str(), OP_SET);
                SETVALUE(lV[4], sPropIDs[P_OCCUPATION], lAttributes[3].c_str(), OP_SET);
                SETVALUE(lV[5], sPropIDs[P_COUNTRY], lAttributes[4].c_str(), OP_SET);
                SETVALUE(lV[6], sPropIDs[P_POSTALCODE], lAttributes[5].c_str(), OP_SET);
                size_t iV;
                for (iV = 0; iV < sizeof(lV) / sizeof(lV[0]); iV++) { lV[iV].meta = META_PROP_NOFTINDEX; }
                lPINs.push_back(pSession.createUncommittedPIN(lV, sizeof(lV) / sizeof(lV[0]), MODE_COPY_VALUES));
                break;
            }
            case 2:
            {
                char lSStr[1024];
                sprintf(lSStr, "SELECT * FROM orgid(%u);", lOrgId);
                IStmt * lS1 = pSession.createStmt(lSStr);
                ICursor * lC; PID lPID1, lPID2; lPID1.ident = lPID2.ident = STORE_OWNER;
                if (RC_OK == lS1->execute(&lC))
                {
                    lC->next(lPID1);
                    lC->destroy();
                    
                    for (TStrings::iterator iS = lRefs.begin(); lRefs.end() != iS; iS++)
                    {
                        sprintf(lSStr, "SELECT * FROM orgid(%s);", (*iS).c_str());
                        IStmt * lS2 = pSession.createStmt(lSStr);
                        if (RC_OK == lS2->execute(&lC))
                        {
                            lC->next(lPID2);
                            lC->destroy();

                            #if 0
                                printf("creating link between "_LX_FM" and "_LX_FM"\n", lPID1.pid, lPID2.pid);
                            #endif

                            // Note: No need to make bidirectional connection here, since the data already contains explicit bidirectional info.
                            Value lV;
                            SETVALUE_C(lV, sPropIDs[P_FRIENDOF], lPID2, OP_ADD, STORE_LAST_ELEMENT);
                            RC lRC = pSession.modifyPIN(lPID1, &lV, 1);
                            if (RC_OK != lRC)
                                printf("ERROR(%d): Couldn't modify PIN "_LX_FM" (%d).\n", __LINE__, (long long unsigned)lPID1.pid, lRC);
                            lRefNum++;
                        }
                        else
                            printf("ERROR(%d): Failed to get pin for orgid=%s.\n", __LINE__, (*iS).c_str());
                        lS2->destroy();
                    }
                }
                else
                    printf("ERROR(%d): Failed to get pin for orgid=%u.\n", __LINE__, lOrgId);
                lS1->destroy();

                if (0 == lRefNum % sTxSize)
                {
                    pSession.commit();
                    pSession.startTransaction();
                }
                break;
            }
            default:
                break;
        }
        
        if (lPINs.size() > sTxSize)
        {
            if (RC_OK != pSession.commitPINs(&lPINs[0], lPINs.size()))
                printf("ERROR(%d): Failed to commit pins\n", __LINE__);
            for (TPINs::iterator iP = lPINs.begin(); lPINs.end() != iP; iP++)
                (*iP)->destroy();
            lPINs.clear();
        }
        lLineNum++;
    }
    printf("\b\b\b\b\b%3d%%", 100);
    if (2 == pPass)
    {
        pSession.commit();
        printf(" created %ld relationships (%ld ms).\n", lRefNum, getTimeInMs() - lT1);
    }
    else if (lPINs.size() > 0 && RC_OK != pSession.commitPINs(&lPINs[0], lPINs.size()))
        printf("ERROR(%d): Failed to commit pins\n", __LINE__);
    else
    {
        for (TPINs::iterator iP = lPINs.begin(); lPINs.end() != iP; iP++)
            (*iP)->destroy();
        printf(" created %ld people (%ld ms).\n", lLineNum, getTimeInMs() - lT1);
    }
}

void addChildToParent(ISession & pSession, char const * pClass, PropertyID pParentPropID, uint32_t pParentID, PID const & pChild)
{
    char lSStr[1024];
    sprintf(lSStr, "SELECT * FROM %s(%u);", pClass, pParentID);
    IStmt * lS = pSession.createStmt(lSStr);
    ICursor * lC;
    if (RC_OK == lS->execute(&lC))
    {
        PID lPIDParent; lPIDParent.ident = STORE_OWNER;
        if (RC_OK != lC->next(lPIDParent))
            printf("ERROR(%d): Couldn't get parent.\n", __LINE__);
        lC->destroy();

        Value lV;
        SETVALUE_C(lV, pParentPropID, pChild, OP_ADD, STORE_LAST_ELEMENT);
        RC lRC = pSession.modifyPIN(lPIDParent, &lV, 1);
        if (RC_OK != lRC)
            printf("ERROR(%d): Couldn't modify PIN "_LX_FM" (%d).\n", __LINE__, (long long unsigned)lPIDParent.pid, lRC);
    }
    else
        printf("ERROR(%d): Failed to get pin for fid=%u.\n", __LINE__, pParentID);
    lS->destroy();
}

void walkCollection(ISession & pSession, Value const * pCollectionProp, TPINs & pCollection)
{
    if (!pCollectionProp)
        return;
    if (VT_REF == pCollectionProp->type)
        pCollection.push_back(pCollectionProp->pin);
    else if (VT_REFID == pCollectionProp->type)
        pCollection.push_back(pSession.getPIN(pCollectionProp->id));
    else if (VT_ARRAY == pCollectionProp->type || VT_COLLECTION == pCollectionProp->type)
    {
        size_t iC = 0;
        Value const * lV = (VT_ARRAY == pCollectionProp->type && iC < pCollectionProp->length) ? &pCollectionProp->varray[iC++] : (VT_COLLECTION == pCollectionProp->type ? pCollectionProp->nav->navigate(GO_FIRST) : NULL);
        while (lV)
        {
            if (VT_REF == lV->type)
                pCollection.push_back(lV->pin);
            else if (VT_REFID == lV->type)
                pCollection.push_back(pSession.getPIN(lV->id));
            lV = (VT_ARRAY == pCollectionProp->type && iC < pCollectionProp->length) ? &pCollectionProp->varray[iC++] : (VT_COLLECTION == pCollectionProp->type ? pCollectionProp->nav->navigate(GO_NEXT) : NULL);
        }
    }
    else
        printf("ERROR(%d): Unexpected value type %d.\n", __LINE__, pCollectionProp->type);
}

void commitProjects(ISession & pSession, TPINs & pProjects, TUint32s & pParentFIDs)
{
    if (RC_OK != pSession.commitPINs(&pProjects[0], pProjects.size()))
        printf("ERROR(%d): Failed to commitPINs.\n", __LINE__);
    pSession.commit(); // REVIEW: Why do I need to do this? (if I don't, the modify below tends to freeze)
    pSession.startTransaction();
    size_t iP;
    for (iP = 0; iP < pProjects.size(); iP++)
    {
        if (0 != pParentFIDs[iP])
        {
            if (iP > 0 && pParentFIDs[iP] >= pProjects[0]->getValue(sPropIDs[P_FID])->ui)
            {
                size_t iPP;
                RC lRC = RC_FALSE;
                for (iPP = 0; iPP < iP && RC_OK != lRC; iPP++)
                {
                    if (pParentFIDs[iP] != pProjects[iPP]->getValue(sPropIDs[P_FID])->ui)
                        continue;
                    Value lV;
                    SETVALUE_C(lV, sPropIDs[P_CHILDREN], pProjects[iP], OP_ADD, STORE_LAST_ELEMENT);
                    lRC = pProjects[iPP]->modify(&lV, 1);
                    break;
                }
                if (RC_OK != lRC)
                    printf("ERROR(%d): Failed to add child project to its parent.\n", __LINE__);
            }
            else
                addChildToParent(pSession, "fid", sPropIDs[P_CHILDREN], pParentFIDs[iP], pProjects[iP]->getPID());
        }
        else
            addChildToParent(pSession, "orgid", sPropIDs[P_ROOTPROJECT], pProjects[iP]->getValue(sPropIDs[P_ACCESS])->ui, pProjects[iP]->getPID());
    }
    for (iP = 0; iP < pProjects.size(); iP++)
        pProjects[iP]->destroy();
    pProjects.clear();
    pParentFIDs.clear();
}

void loadProjects(ISession & pSession)
{
    long const lTotalLineNum = countLines(sProjectsFN);
    std::ifstream lIs(sProjectsFN);
    char lLine[32768];
    long lLineNum = 0;
    printf("%3d%%", 0); fflush(stdout);
    long const lT1 = getTimeInMs();
    pSession.startTransaction();
    TPINs lProjects;
    TUint32s lParentFIDs;
    while (lIs.is_open() && !lIs.eof())
    {
        lIs.getline(lLine, sizeof(lLine));
        if (0 == strlen(lLine))
            continue;
        if (0 == lLineNum % 1000)
            { printf("\b\b\b\b\b%3d%%", int(1.0 + 100.0 * lLineNum / lTotalLineNum)); fflush(stdout); }
        
        // Parse the line.
        // (2 'umquam' 1 20)
        uint32_t lFID, lParentFID, lAccessOrgID;
        if (1 != sscanf(&lLine[1], "%u", &lFID))
            printf("ERROR(%d): Couldn't read fid.\n", __LINE__);
        TStrings lAttributes;
        char * lQuote1 = strchr(lLine, '\'');
        char * lQuote2 = strchr(lQuote1 + 1, '\'');
        lAttributes.push_back(std::string(lQuote1 + 1, lQuote2 - lQuote1 - 1));
        if (2 != sscanf(lQuote2 + 2, "%u %u", &lParentFID, &lAccessOrgID))
            printf("ERROR(%d): Couldn't read expected number of attributes from %s.\n", __LINE__, lQuote2 + 2);
        
        Value lV[3];
        SETVALUE(lV[0], sPropIDs[P_FID], lFID, OP_SET);
        SETVALUE(lV[1], sPropIDs[P_FNAME], lAttributes[0].c_str(), OP_SET);
        SETVALUE(lV[2], sPropIDs[P_ACCESS], lAccessOrgID, OP_SET); // Review: maybe by reference instead...
        lV[1].meta = META_PROP_NOFTINDEX;
        lProjects.push_back(pSession.createUncommittedPIN(lV, sizeof(lV) / sizeof(lV[0]), MODE_COPY_VALUES));
        if (!lProjects.back())
            printf("ERROR(%d): Failed to createUncommittedPIN.\n", __LINE__);
        lParentFIDs.push_back(lParentFID);
        if (0 == lLineNum % sTxSize)
        {
            commitProjects(pSession, lProjects, lParentFIDs);
            pSession.commit();
            pSession.startTransaction();
        }
        lLineNum++;
    }
    commitProjects(pSession, lProjects, lParentFIDs);
    pSession.commit();
    printf("\b\b\b\b\b%3d%%", 100);
    printf(" created %ld projects (%ld ms).\n", lLineNum, getTimeInMs() - lT1);
}

void commitPhotos(ISession & pSession, TPINs & pPhotos, TUint32s & pParentFIDs)
{
    if (RC_OK != pSession.commitPINs(&pPhotos[0], pPhotos.size()))
        printf("ERROR(%d): Failed to commitPINs.\n", __LINE__);
    size_t iP;
    for (iP = 0; iP < pPhotos.size(); iP++)
    {
        addChildToParent(pSession, "fid", sPropIDs[P_CHILDREN], pParentFIDs[iP], pPhotos[iP]->getPID());
        pPhotos[iP]->destroy();
    }
    pPhotos.clear();
    pParentFIDs.clear();
}

void loadPhotos(ISession & pSession)
{
    long const lTotalLineNum = countLines(sPhotosFN);
    std::ifstream lIs(sPhotosFN);
    char lLine[32768];
    long lLineNum = 0;
    printf("%3d%%", 0); fflush(stdout);
    long const lT1 = getTimeInMs();
    pSession.startTransaction();
    TPINs lPhotos;
    TUint32s lParentFIDs;
    while (lIs.is_open() && !lIs.eof())
    {
        lIs.getline(lLine, sizeof(lLine));
        if (0 == strlen(lLine))
            continue;
        if (0 == lLineNum % 1000)
            { printf("\b\b\b\b\b%3d%%", int(1.0 + 100.0 * lLineNum / lTotalLineNum)); fflush(stdout); }
        
        // Parse the line.
        // (37 'pink76610' 26)
        uint32_t lFID, lParentFID;
        if (1 != sscanf(&lLine[1], "%u", &lFID))
            printf("ERROR(%d): Couldn't read fid.\n", __LINE__);
        TStrings lAttributes;
        char * lQuote1 = strchr(lLine, '\'');
        char * lQuote2 = strchr(lQuote1 + 1, '\'');
        lAttributes.push_back(std::string(lQuote1 + 1, lQuote2 - lQuote1 - 1));
        if (1 != sscanf(lQuote2 + 2, "%u", &lParentFID))
            printf("ERROR(%d): Couldn't read expected number of attributes from %s.\n", __LINE__, lQuote2 + 2);
        
        Value lV[2];
        SETVALUE(lV[0], sPropIDs[P_FID], lFID, OP_SET);
        SETVALUE(lV[1], sPropIDs[P_PNAME], lAttributes[0].c_str(), OP_SET);
        lV[1].meta = META_PROP_NOFTINDEX;
        lPhotos.push_back(pSession.createUncommittedPIN(lV, sizeof(lV) / sizeof(lV[0]), MODE_COPY_VALUES));
        if (!lPhotos.back())
            printf("ERROR(%d): Failed to createUncommittedPIN.\n", __LINE__);
        lParentFIDs.push_back(lParentFID);
        if (0 == lLineNum % sTxSize)
        {
            commitPhotos(pSession, lPhotos, lParentFIDs);
            pSession.commit();
            pSession.startTransaction();
        }
        lLineNum++;
    }
    commitPhotos(pSession, lPhotos, lParentFIDs);
    pSession.commit();
    printf("\b\b\b\b\b%3d%%", 100);
    printf(" created %ld photos (%ld ms).\n", lLineNum, getTimeInMs() - lT1);
}

bool dfSearch(ISession & pSession, IPIN & pFrom, TPIDs & pVisited, uint32_t pP1, uint32_t pP2)
{
    PID const lPID = pFrom.getPID();
    if (pVisited.end() != pVisited.find(lPID))
        return false;
    pVisited.insert(lPID);
    Value const * const lColl = pFrom.getValue(sPropIDs[P_FRIENDOF]);
    TPINs lFriends;
    walkCollection(pSession, lColl, lFriends);
    TPINs::iterator iF;
    bool lFound = false;
    for (iF = lFriends.begin(); lFriends.end() != iF && !lFound; iF++)
        if ((*iF)->getValue(sPropIDs[P_ORGID])->ui == pP2)
            { printf("%u can reach %u from %u ", pP1, pP2, pP2); lFound = true; }
    for (iF = lFriends.begin(); lFriends.end() != iF && !lFound; iF++)
        if (dfSearch(pSession, *(*iF), pVisited, pP1, pP2))
            { printf(" from %u", (*iF)->getValue(sPropIDs[P_ORGID])->ui); lFound = true; }
    for (iF = lFriends.begin(); lFriends.end() != iF; iF++)
        (*iF)->destroy();
    return lFound;
}
typedef std::map<uint32_t, uint32_t> TUint32map;
bool bfSearch(ISession & pSession, TPINs const & pPINs, TPIDs & pVisited, TUint32map & pBwHops, uint32_t pP1, uint32_t pP2)
{
    TPINs lNextLevel;
    TPINs::const_iterator iP;
    bool lFound = false;
    for (iP = pPINs.begin(); pPINs.end() != iP && !lFound; iP++)
    {
        Value const * const lColl = (*iP)->getValue(sPropIDs[P_FRIENDOF]);
        uint32_t const lFromOrgid = (*iP)->getValue(sPropIDs[P_ORGID])->ui;
        TPINs lFriends;
        walkCollection(pSession, lColl, lFriends);
        TPINs::iterator iF;
        for (iF = lFriends.begin(); lFriends.end() != iF && !lFound; iF++)
        {
            uint32_t const lToOrgid = (*iF)->getValue(sPropIDs[P_ORGID])->ui;
            if (lToOrgid == pP2)
                { printf("%u can reach %u", pP1, pP2); lFound = true; }
            PID const lToPID = (*iF)->getPID();
            if (pVisited.end() == pVisited.find(lToPID))
            {
                pBwHops[lToOrgid] = lFromOrgid;
                lNextLevel.push_back(*iF);
                pVisited.insert(lToPID);
            }
        }
    }
    if (!lFound && lNextLevel.size() > 0 && bfSearch(pSession, lNextLevel, pVisited, pBwHops, pP1, pP2))
        lFound = true;
    for (iP = lNextLevel.begin(); lNextLevel.end() != iP; iP++)
        (*iP)->destroy();
    return lFound;
}

void runQueries(ISession & pSession)
{
    long lTotalTime = 0;
    IStmt * lS = pSession.createStmt("SELECT * FROM orgid;");
    uint64_t lNumPeople;
    lS->count(lNumPeople);
    lS->destroy();
    printf("%lu people\n", (unsigned long)lNumPeople);
    uint32_t lP1 = 10, lP2 = (uint32_t)lNumPeople - 10;
    char lStmt[2048];
    for (int iT = 0; iT < 20 && lP1 < lNumPeople && lP2 > 0; iT++)
    {
        // Case 1: can person1 reach person2?
        long const lT1 = getTimeInMs();
        TPINs lToVisit;
        TPIDs lVisited;
        TUint32map lBwHops;

        sprintf(lStmt, "SELECT * FROM orgid(%u);", lP1);
        lS = pSession.createStmt(lStmt);
        ICursor * lC;
        if (RC_OK == lS->execute(&lC))
        {
            lToVisit.push_back(lC->next());
            lVisited.insert(lToVisit.back()->getPID());
            lC->destroy();
            lS->destroy();
        }

        if (bfSearch(pSession, lToVisit, lVisited, lBwHops, lP1, lP2))
        {
            // Q: would there exist a better algorithm that can't be implemented easily with the rdb interface?
            //    judging from "Fast and Practical Indexing and Querying of Very Large Graphs"(Trissl&Leser) etc., seems hopeless...
            uint32_t iFrom = lBwHops[lP2];
            while (iFrom != lP1)
            {
                printf(" from %u", iFrom);
                iFrom = lBwHops[iFrom];
            }
            printf(" from %u\n", lP1);
            lToVisit.back()->destroy();
        }
        else
            printf("%u cannot reach %u\n", lP1, lP2);

        lP1 += 10; lP2 -= 10;
        lTotalTime += getTimeInMs() - lT1;
    }
    printf("done (%ld ms).\n", lTotalTime);

    lS = pSession.createStmt("SELECT * FROM orgid([10, 15]);");
    ICursor * lC;
    IPIN * lPin;
    if (RC_OK == lS->execute(&lC))
    {
        lTotalTime = 0;
        for (lPin = lC->next(); NULL != lPin; lPin = lC->next())
        {
            // Case 2: find the set of all people that have friends among A's friends
            long const lT1 = getTimeInMs();
            PID const lPID1 = lPin->getPID();
            Value const * const lColl = lPin->getValue(sPropIDs[P_FRIENDOF]);
            TPINs lFriends1;
            TPINs::iterator iF;
            walkCollection(pSession, lColl, lFriends1);
            #if 0
                printf("%s %s %s has %ld friends\n", lPin->getValue(sPropIDs[P_FIRSTNAME])->str, lPin->getValue(sPropIDs[P_MIDDLENAME])->str, lPin->getValue(sPropIDs[P_LASTNAME])->str, (long)lFriends1.size());
                for (iF = lFriends1.begin(); lFriends1.end() != iF; iF++)
                    printf("  friend (of %s %s %s): %s %s %s\n", lPin->getValue(sPropIDs[P_FIRSTNAME])->str, lPin->getValue(sPropIDs[P_MIDDLENAME])->str, lPin->getValue(sPropIDs[P_LASTNAME])->str, (*iF)->getValue(sPropIDs[P_FIRSTNAME])->str, (*iF)->getValue(sPropIDs[P_MIDDLENAME])->str, (*iF)->getValue(sPropIDs[P_LASTNAME])->str);                  
            #endif
            TPIDs lFof;
            for (iF = lFriends1.begin(); lFriends1.end() != iF; iF++)
            {
                if (NULL == *iF)
                    continue;
                size_t iC = 0;
                PID const lPID2 = (*iF)->getPID();
                Value const * const lColl2 = (*iF)->getValue(sPropIDs[P_FRIENDOF]);
                Value const * lV = (VT_ARRAY == lColl2->type && iC < lColl2->length) ? &lColl2->varray[iC++] : (VT_COLLECTION == lColl2->type ? lColl2->nav->navigate(GO_FIRST) : NULL);
                while (lV)
                {
                    PID lPID3;
                    if (VT_REF == lV->type)
                        lPID3 = lV->pin->getPID();
                    else if (VT_REFID == lV->type)
                        lPID3 = lV->id;
                    if (lPID1 != lPID3 && lPID2 != lPID3)
                        lFof.insert(lPID3);
                    lV = (VT_ARRAY == lColl2->type && iC < lColl2->length) ? &lColl2->varray[iC++] : (VT_COLLECTION == lColl2->type ? lColl2->nav->navigate(GO_NEXT) : NULL);
                }
            }
            for (iF = lFriends1.begin(); lFriends1.end() != iF; iF++)
                (*iF)->destroy();
            lTotalTime += getTimeInMs() - lT1;
            printf("%ld people have friends in common with %s %s %s\n", (long)lFof.size(), lPin->getValue(sPropIDs[P_FIRSTNAME])->str, lPin->getValue(sPropIDs[P_MIDDLENAME])->str, lPin->getValue(sPropIDs[P_LASTNAME])->str);
            #if 0
                for (TPIDs::iterator iFof = lFof.begin(); lFof.end() != iFof; iFof++)
                {
                    IPIN * lFofp = pSession.getPIN(*iFof);
                    printf("  %s %s %s\n", lFofp->getValue(sPropIDs[P_FIRSTNAME])->str, lFofp->getValue(sPropIDs[P_MIDDLENAME])->str, lFofp->getValue(sPropIDs[P_LASTNAME])->str);
                    lFofp->destroy();
                }
            #endif
            lPin->destroy();
        }
        printf("done (%ld ms).\n", lTotalTime);

        lTotalTime = 0;
        lC->rewind();
        for (lPin = lC->next(); NULL != lPin; lPin = lC->next())
        {
            // Case 3: find all photos owned by A, i.e. present in the project structure owned by A
            long const lT1 = getTimeInMs();
            long lNumPhotos = 0;
            Value const * const lRootProjectv = lPin->getValue(sPropIDs[P_ROOTPROJECT]);
            if (lRootProjectv)
            {
                TPINs lStack;
                lStack.push_back(pSession.getPIN(lRootProjectv->id));
                while (!lStack.empty())
                {
                    IPIN * lP = lStack.back(); lStack.pop_back();
                    Value const * const lChildren = lP->getValue(sPropIDs[P_CHILDREN]);
                    if (lChildren)
                        walkCollection(pSession, lChildren, lStack);
                    else if (lP->getValue(sPropIDs[P_PNAME]))
                        lNumPhotos += 1;
                    lP->destroy();
                }
            }
            lTotalTime += getTimeInMs() - lT1;
            printf("%s %s %s has projects containing %ld photos\n", lPin->getValue(sPropIDs[P_FIRSTNAME])->str, lPin->getValue(sPropIDs[P_MIDDLENAME])->str, lPin->getValue(sPropIDs[P_LASTNAME])->str, lNumPhotos);
            lPin->destroy();
        }
        printf("done (%ld ms).\n", lTotalTime);

        lTotalTime = 0;
        lC->rewind();
        for (lPin = lC->next(); NULL != lPin; lPin = lC->next())
        {
            // Case 4: find all photos given access to A by his friends
            long const lT1 = getTimeInMs();
            long lNumPhotos = 0;
            #if 0
                Value const * const lColl = lPin->getValue(sPropIDs[P_FRIENDOF]);
                TPINs lFriends;
                TPINs::iterator iF;
                walkCollection(pSession, lColl, lFriends);
                for (iF = lFriends.begin(); lFriends.end() != iF; iF++)
                {
                    if (NULL == *iF)
                        continue;
                    Value const * const lRootProjectv = (*iF)->getValue(sPropIDs[P_ROOTPROJECT]);
                    TPINs lStack;
                    lStack.push_back(pSession.getPIN(lRootProjectv->id));
                    while (!lStack.empty())
                    {
                        IPIN * lP = lStack.back(); lStack.pop_back();
                        Value const * const lChildren = lP->getValue(sPropIDs[P_CHILDREN]);
                        if (lChildren)
                        {
                            TPINs lTmpStack;
                            walkCollection(pSession, lChildren, lTmpStack);
                            TPINs::iterator iTS;
                            Value const * lVAccess = lP->getValue(sPropIDs[P_ACCESS]);
                            for (iTS = lTmpStack.begin(); lTmpStack.end() != iTS; iTS++)
                            {
                                if ((*iTS)->getValue(sPropIDs[P_PNAME]))
                                    if (!lVAccess || lVAccess->ui != lPin->getValue(sPropIDs[P_ORGID])->ui)
                                        continue;
                                lStack.push_back(*iTS);
                            }
                        }
                        else if (lP->getValue(sPropIDs[P_PNAME]))
                            lNumPhotos += 1;
                        lP->destroy();
                    }
                }
            #else
                IStmt * lS2 = pSession.createStmt("SELECT * FROM :0.friendof.rootproject.children{*}[access=:1].children[exists(pname)]");
                if (lS2)
                {
                    Value params[2]; params[0].set(lPin->getPID()); params[1].set((unsigned)lPin->getValue(sPropIDs[P_ORGID])->ui);
                    uint64_t lNP;
                    lS2->count(lNP,params,2);
                    lNumPhotos += lNP;
                    lS2->destroy();
                }
                else
                    printf("ERROR(%d): Couldn't createStmt %s\n", __LINE__, "SELECT * FROM :0.friendof.rootproject.children{*}[access=:1].children[NOT exists(children) AND exists(pname)]");
            #endif
            lTotalTime += getTimeInMs() - lT1;
            printf("%s %s %s has access to %ld photos shared by friends\n", lPin->getValue(sPropIDs[P_FIRSTNAME])->str, lPin->getValue(sPropIDs[P_MIDDLENAME])->str, lPin->getValue(sPropIDs[P_LASTNAME])->str, lNumPhotos);
            lPin->destroy();
        }
        printf("done (%ld ms).\n", lTotalTime);
        lC->destroy();
    }
    lS->destroy();
}

ClassID createClass(ISession * pSession, char const * pName, IStmt * pPredicate)
{
    ClassID lClsid = STORE_INVALID_CLASSID;
    Value vals[2];
    vals[0].set(pName); vals[0].setPropID(PROP_SPEC_URI);
    vals[1].set(pPredicate); vals[1].setPropID(PROP_SPEC_PREDICATE);
    IPIN * lP = pSession->createUncommittedPIN(vals, 2, MODE_COPY_VALUES);
    if (lP)
    {
        RC lRC = pSession->commitPINs(&lP, 1);
        if (RC_OK == lRC || RC_ALREADYEXISTS == lRC)
            lClsid = lP->getValue(PROP_SPEC_CLASSID)->uid;
        lP->destroy();
    }
    pPredicate->destroy();
    return lClsid;
}

// Publish this test.
class TestGraph1 : public ITest
{
    public:
        TEST_DECLARE(TestGraph1);
        virtual char const * getName() const { return "testgraph1"; }
        virtual char const * getHelp() const { return ""; }
        virtual char const * getDescription() const { return "quick&dirty test environment for new path expressions"; }
        virtual bool includeInSmokeTest(char const *& pReason) const { pReason = "work in progress"; return false; }
        virtual bool includeInBashTest(char const *& pReason) const { pReason = "work in progress"; return false; }
        
        virtual int execute();
        virtual void destroy() { delete this; }
};
TEST_IMPLEMENT(TestGraph1, TestLogger::kDStdOut);

// Implement this test.
int TestGraph1::execute()
{
    bool lSuccess = true;
    if (MVTApp::startStore())
    {
        ISession * const lSession = MVTApp::startSession();

        // Map properties.
        size_t iP;
        for (iP = 0; iP < sizeof(sProps) / sizeof(sProps[0]); iP++)
        {
            URIMap lUM; lUM.URI = sProps[iP]; lUM.uid = STORE_INVALID_PROPID;
            if (RC_OK != lSession->mapURIs(1, &lUM)) printf("ERROR(%d): Couldn't map property %s\n", __LINE__, sProps[iP]);
                sPropIDs[iP] = lUM.uid;
        }

        // Create classes.
        if (RC_NOTFOUND == lSession->getClassID("orgid", sClsid_orgid))
            sClsid_orgid = createClass(lSession, "orgid", lSession->createStmt("select * where $0 in :0;", &sPropIDs[P_ORGID], 1));
        if (RC_NOTFOUND == lSession->getClassID("fid", sClsid_fid))
            sClsid_fid = createClass(lSession, "fid", lSession->createStmt("select * where $0 in :0;", &sPropIDs[P_FID], 1));

        // Run the benchmark.
        #if 1
            loadPeople(*lSession, 1);
            loadPeople(*lSession, 2);
            loadProjects(*lSession);
            loadPhotos(*lSession);
        #endif
        runQueries(*lSession);

        // Cleanup.        
        lSession->terminate();
        MVTApp::stopStore();
    }
    else { TVERIFY(!"could not open store") ; }
    return lSuccess ? 0 : 1;
}
