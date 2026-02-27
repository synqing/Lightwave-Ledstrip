/**
 * @file test_http_palette_codec.cpp
 * @brief Unit tests for HttpPaletteCodec JSON parsing and encoder allow-list validation
 *
 * Tests HTTP palette endpoint decoding and encoding allow-lists.
 *
 * @author LightwaveOS Team
 * @version 2.0.0
 */

#ifdef NATIVE_BUILD

#include <unity.h>
#include <ArduinoJson.h>
#include <cstring>
#include "../../src/codec/HttpPaletteCodec.h"

using namespace lightwaveos::codec;

static bool validateKeysAgainstAllowList(const JsonObject& obj, const char* allowedKeys[], size_t allowedCount) {
    size_t foundCount = 0;
    for (JsonPair kv : obj) {
        const char* key = kv.key().c_str();
        bool isAllowed = false;
        for (size_t i = 0; i < allowedCount; i++) {
            if (strcmp(key, allowedKeys[i]) == 0) {
                isAllowed = true;
                foundCount++;
                break;
            }
        }
        if (!isAllowed) {
            return false;
        }
    }
    for (size_t i = 0; i < allowedCount; i++) {
        if (!obj.containsKey(allowedKeys[i])) {
            return false;
        }
    }
    return foundCount == allowedCount;
}

void test_palette_decode_set_valid() {
    JsonDocument doc;
    doc["paletteId"] = 7;
    HttpPaletteSetDecodeResult result = HttpPaletteCodec::decodeSet(doc.as<JsonObjectConst>());
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_EQUAL_UINT8(7, result.request.paletteId);
}

void test_palette_encode_list_allowlist() {
    HttpPaletteItemData paletteItems[1];
    paletteItems[0].paletteId = 3;
    paletteItems[0].name = "Test";
    paletteItems[0].category = "Artistic";
    paletteItems[0].flags.warm = true;
    paletteItems[0].flags.cvdFriendly = true;
    paletteItems[0].avgBrightness = 120;
    paletteItems[0].maxBrightness = 255;

    HttpPalettesListPaginationData pagination;
    pagination.total = 10;
    pagination.offset = 0;
    pagination.limit = 5;

    HttpPalettesListCompatPaginationData compat;
    compat.page = 1;
    compat.limit = 5;
    compat.total = 10;
    compat.pages = 2;

    HttpPaletteCategoryCounts categories;
    categories.artistic = 3;
    categories.scientific = 2;
    categories.lgpOptimized = 1;

    HttpPalettesListData listData;
    listData.pagination = pagination;
    listData.compatPagination = compat;
    listData.categories = categories;
    listData.palettes = paletteItems;
    listData.paletteCount = 1;
    listData.count = 1;

    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    HttpPaletteCodec::encodeList(listData, obj);

    const char* allowedKeys[] = {
        "total", "offset", "limit", "pagination", "categories", "palettes", "count"
    };
    TEST_ASSERT_TRUE(validateKeysAgainstAllowList(obj, allowedKeys, sizeof(allowedKeys) / sizeof(allowedKeys[0])));
    TEST_ASSERT_TRUE(obj.containsKey("palettes"));
    TEST_ASSERT_EQUAL_UINT(1, obj["palettes"].size());
}

void test_palette_encode_item_allowlist() {
    HttpPaletteItemData item;
    item.paletteId = 1;
    item.name = "Test";
    item.category = "Artistic";
    item.flags.warm = true;
    item.flags.cool = false;
    item.flags.calm = true;
    item.flags.vivid = false;
    item.flags.cvdFriendly = true;
    item.flags.whiteHeavy = false;
    item.avgBrightness = 100;
    item.maxBrightness = 200;

    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    HttpPaletteCodec::encodePaletteItem(item, obj);

    const char* allowedKeys[] = {
        "paletteId", "name", "category", "flags", "avgBrightness", "maxBrightness"
    };
    TEST_ASSERT_TRUE(validateKeysAgainstAllowList(obj, allowedKeys, sizeof(allowedKeys) / sizeof(allowedKeys[0])));
}

void setUp(void) {}
void tearDown(void) {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_palette_decode_set_valid);
    RUN_TEST(test_palette_encode_list_allowlist);
    RUN_TEST(test_palette_encode_item_allowlist);
    return UNITY_END();
}

#endif
