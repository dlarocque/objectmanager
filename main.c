#include "ObjectManager.h"
#include <stdio.h>

typedef enum {FALSE, TRUE} Boolean;

typedef struct TESTCASE {
    char testName[100];
    ulong expected;
    int size;
} testCases; 

// PROTOTYPES
static void testPools();
static void testInsert();
static void testRetrieval();
static void testModReferences();
static void testGarbageCollector();

int main(int argc, char *argv[]) {
    printf("---------------------\nTesting Object Manager\n---------------------\n\n");
    
    // We test initPool and destroyPool together, as testing initPool correctly
    // is not possible if destroyPool is not working correctly.
    testPools(); // covers all test cases for init and destroy pool;    
     
    // While this does use reference modifications to test, we cannot test 
    // the reference modifiers without testing insert first. 
    testInsert();
    
    // We test the reference modifiers before garbage collection and retrieval
    // since they both depend on dropping references to 0 in some cases.
    testModReferences();

    testGarbageCollector();

    testRetrieval();
    
    printf("\n---------------------\nTesting Complete.\n---------------------\n");
}


static void testPools() { 
    printf("**Testing initPool and destroyPool**\n");
    
    // GENERAL CASES
    printf("Initializing Pool...\n");
    initPool();
    printf("Pool initialized, should be no objects in pool : \n");
    dumpPool();
    
    printf("Destroying Pool...\n");
    destroyPool();
    printf("Pool destroyed.\n");

    // EDGE CASES
    // Edge cases are simply initializing and destroying a second time
    printf("Initializing Pool...\n");
    initPool();
    printf("Pool initialized, should be no objects in pool\n");
    dumpPool();

    printf("Destroying Pool...\n");
    destroyPool();
    printf("Pool destroyed.\n\n");


}

// Tests insertObject method (and compact too i guess)
#define INSERT_TESTS 7
static void testInsert() {
    initPool();
    Ref ids[INSERT_TESTS];
    testCases tests[INSERT_TESTS] = {{"Inserting into empty buffer", 1, 5},
                                     {"Inserting into nonempty buffer", 2, 500},
                                     {"Inserting into unfragmented buffer", 3, 5000},
                                     {"Inserting into full, unfragmented buffer",0, 519000},
                                     {"Inserting into fragmented buffer", 4, 1},
                                     {"Inserting into full, fragmented buffer", 5, 519000},
                                     {"Inserting object that is too large", 0, 70000},
                                     };

    printf("\nTesting insertion\n--------------------------\n\n");    
        
    for(int i = 0; i < INSERT_TESTS ; i++) {
        printf("\n%s : ", tests[i].testName);
        
        if(i == 4) {
            //fragment buffer by dropping a ref to 0
            dropReference(ids[2]);
        }
        ids[i] = insertObject(tests[i].size);
        
        if(ids[i] == tests[i].expected) {
                printf("\nPASSED!");
        } else {
            printf("\nFAILED!");
        }

        dumpPool();
    }
    destroyPool();
}

#define GC_TESTS 6
static void testGarbageCollector() {
    printf("\n Testing garbage collection\n----------------\n\n");
    
    // We test the garbage collector manually by looking through all of the cases
    // listed here in stdout to make sure they're working as they should.

    // We need to test all of the cases where the garbage collector is called.
    printf("Let X represent objects with no references, O are objects with references\n\n");

    testCases tests[GC_TESTS] = {{"X -> O -> O", 1, 3},
                                 {"X -> X -> O", 2, 3},
                                 {"X -> X -> X", 3, 3},
                                 {"O -> X -> X", 2, 3},
                                 {"O -> X -> O", 1, 3},
                                 {"O -> O -> X", 1, 3}};

    // indexes to dropReferences to 0 in each test
    int toRemove[GC_TESTS][3] = {{1},{1,2},{1,2,3},{2,3},{2},{3}};

    for(int i = 0; i < GC_TESTS; i++) {
        printf("\n%s\n", tests[i].testName);

        initPool();

        // add tests[i].size objects to buffer
        for(int j = 0; j < tests[i].size; j++) {
            insertObject(100000);
        }
    
        // decrement tests[i].expected ids to 0
        for(int j = 0; j < tests[i].expected; j++) {
            dropReference(toRemove[i][j]);
        }
        dumpPool(); // pool before collection

        insertObject(300000); // to call compact
        dumpPool(); // pool after collection

        destroyPool();
    }
}

static void testRetrieval() {
    initPool();
    printf("\nTesting Retrieval\n-------------------\n\n");

    printf("General Cases\n\n");
    
    printf("Retrieve existing, non-garbage object\n");
    Ref id1 = insertObject(100);
    char *ptr = (char *)retrieveObject(id1);

    printf("Should be valid address: %p\n", ptr);
    
    printf("Storing A-Z in object\n");
    for(int i = 65; i < 91; i++) {
        ((uchar*)ptr)[i] = (char)i;
        printf("%c" ,(char)((uchar*)ptr)[i]);
    }

    dumpPool();
    dropReference(id1);
    dumpPool();
    
    printf("Removed reference to object, retrieve should return NULL, terminate pool.\n");
    retrieveObject(id1);
    destroyPool();

    initPool();
    id1 = insertObject(500);
    dropReference(id1);
    insertObject(523800); // to trigger compact()
    printf("Object should have now been cleaned from the garbage collector.\n");
    printf("\nRetrieving object that was cleaned from garbage collector... Should terminate.\n");
    retrieveObject(id1);

    destroyPool();
}

static void testModReferences() {
    
    initPool();
    printf("\nTesting Reference modifiers\n------------------------\n\n");
    
    Ref id1 = insertObject(100);
    printf("\nInserted object of size 100, ref count should be 1\n");
    dumpPool();
    addReference(id1);
    printf("\nAdded reference to object, ref count should be 2\n");
    dumpPool();

    dropReference(id1);
    printf("\nDropped reference to object, ref count should be 1\n");
    dumpPool();
    
    dropReference(id1);
    printf("\nDropped reference to object, ref count should be 0\n");
    dumpPool();
    
    dropReference(id1);
    printf("\nDropped reference to object, ref count should be 0\n");
    dumpPool();

    Ref id2 = insertObject(300);
    printf("\nAdded object of size 300, incrementing second object in list, ref count should be 2");
    addReference(id2);
    dumpPool();

    printf("Adding, dropping reference to non-existent object, should not do anything");
    dropReference(5);
    addReference(5); // does not exist
    dumpPool();

    destroyPool();
}
