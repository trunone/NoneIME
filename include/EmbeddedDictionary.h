#pragma once

#include <cstdint>
#include <cstddef>

#pragma pack(push, 1)
struct DictIndexEntry
{
    char key[6];            // Null-terminated key string (max length 5)
    uint16_t phraseOffset;  // Start index in g_dictPhrases
    uint16_t phraseCount;   // Number of candidate phrases for this key
};
#pragma pack(pop)

extern const DictIndexEntry g_dictIndex[];
extern const size_t g_dictIndexCount;

extern const wchar_t g_dictPhrases[];
extern const size_t g_dictPhrasesCount;
