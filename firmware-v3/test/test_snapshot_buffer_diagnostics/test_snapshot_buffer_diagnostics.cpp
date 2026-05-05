#include <unity.h>

#include "audio/contracts/SnapshotBuffer.h"

using lightwaveos::audio::SnapshotBuffer;

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

}  // namespace

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_snapshot_buffer_reports_payload_storage_address);
    RUN_TEST(test_snapshot_buffer_reports_payload_and_object_sizes);
    return UNITY_END();
}
