#include <unity.h>
#include "network/SubscriptionManager.h"

using namespace lightwaveos::network;

void test_subscription_manager_add() {
    SubscriptionManager<2> sub;
    TEST_ASSERT_EQUAL(0, sub.count());
    TEST_ASSERT_TRUE(sub.add(10));
    TEST_ASSERT_EQUAL(1, sub.count());
    TEST_ASSERT_TRUE(sub.contains(10));
    TEST_ASSERT_EQUAL(10, sub.get(0));
}

void test_subscription_manager_full() {
    SubscriptionManager<2> sub;
    TEST_ASSERT_TRUE(sub.add(1));
    TEST_ASSERT_TRUE(sub.add(2));
    TEST_ASSERT_EQUAL(2, sub.count());
    TEST_ASSERT_FALSE(sub.add(3)); // Should be full
    TEST_ASSERT_EQUAL(2, sub.count());
}

void test_subscription_manager_duplicate() {
    SubscriptionManager<2> sub;
    TEST_ASSERT_TRUE(sub.add(1));
    TEST_ASSERT_TRUE(sub.add(1)); // Duplicate add returns true (idempotent success)
    TEST_ASSERT_EQUAL(1, sub.count());
}

void test_subscription_manager_remove() {
    SubscriptionManager<3> sub;
    sub.add(1);
    sub.add(2);
    sub.add(3);
    
    // Remove middle
    TEST_ASSERT_TRUE(sub.remove(2));
    TEST_ASSERT_EQUAL(2, sub.count());
    TEST_ASSERT_FALSE(sub.contains(2));
    TEST_ASSERT_TRUE(sub.contains(1));
    TEST_ASSERT_TRUE(sub.contains(3));
    
    // Remove non-existent
    TEST_ASSERT_FALSE(sub.remove(99));
    TEST_ASSERT_EQUAL(2, sub.count());
}

void test_subscription_manager_clear() {
    SubscriptionManager<2> sub;
    sub.add(1);
    sub.clear();
    TEST_ASSERT_EQUAL(0, sub.count());
}

void run_subscription_manager_tests() {
    RUN_TEST(test_subscription_manager_add);
    RUN_TEST(test_subscription_manager_full);
    RUN_TEST(test_subscription_manager_duplicate);
    RUN_TEST(test_subscription_manager_remove);
    RUN_TEST(test_subscription_manager_clear);
}
