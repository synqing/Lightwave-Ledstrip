#include <unity.h>

#include "audio/contracts/SnapshotBuffer.h"

using lightwaveos::audio::SnapshotBuffer;
using lightwaveos::audio::InternalSnapshotBufferOwner;

namespace {

void test_snapshot_buffer_reports_payload_storage_address() {
    SnapshotBuffer<uint32_t> buffer;

    TEST_ASSERT_NOT_NULL(buffer.StorageAddressForDiagnostics());
}

void test_snapshot_buffer_reports_payload_and_object_sizes() {
    SnapshotBuffer<uint32_t> buffer;

    TEST_ASSERT_EQUAL_UINT32(sizeof(uint32_t) * 2U,
                             buffer.PayloadBytesForDiagnostics());
    TEST_ASSERT_EQUAL_UINT32(sizeof(buffer),
                             buffer.ObjectBytesForDiagnostics());
}

void test_internal_snapshot_buffer_owner_constructs_buffer() {
    InternalSnapshotBufferOwner<uint32_t> owner;

    TEST_ASSERT_TRUE(owner.IsReady());
    TEST_ASSERT_NOT_NULL(owner.get());
    TEST_ASSERT_EQUAL_UINT32(sizeof(uint32_t) * 2U,
                             owner->PayloadBytesForDiagnostics());
}

}  // namespace

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_snapshot_buffer_reports_payload_storage_address);
    RUN_TEST(test_snapshot_buffer_reports_payload_and_object_sizes);
    RUN_TEST(test_internal_snapshot_buffer_owner_constructs_buffer);
    return UNITY_END();
}
