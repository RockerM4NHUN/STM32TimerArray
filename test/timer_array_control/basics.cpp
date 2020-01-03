# include "../hwsetup.h"

#include <unity.h>

void test_evaluation_of_an_empty_test(){
    // empty test should pass
}

void hwsetup(){
    hwsetup_init();
}

int main() {

    hwsetup();

    UNITY_BEGIN();

    RUN_TEST(test_evaluation_of_an_empty_test);

    UNITY_END(); // stop unit testing

    while(1){}
}
