#include <unity.h>

#include "../test_native/test_perceptual_jnd.cpp"

int main(int, char**) {
    UNITY_BEGIN();
    run_perceptual_jnd_tests();
    return UNITY_END();
}
