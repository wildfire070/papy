// Hyphenation library unit tests
// Tests the Liang algorithm hyphenation with serialized trie patterns,
// language registry, common helper functions, and the public Hyphenation API.

#include "test_utils.h"

#include <Hyphenation/HyphenationCommon.h>
#include <Hyphenation/Hyphenation.h>
#include <Hyphenation/Hyphenator.h>
#include <Hyphenation/LanguageRegistry.h>

#include <string>
#include <vector>

// Helper: collect break offsets as a simple byte-offset vector
static std::vector<size_t> breakByteOffsets(const std::string& word, bool fallback = false) {
  auto breaks = Hyphenation::breakOffsets(word, fallback);
  std::vector<size_t> offsets;
  offsets.reserve(breaks.size());
  for (const auto& b : breaks) {
    offsets.push_back(b.byteOffset);
  }
  return offsets;
}

// Helper: split word at break points to visualize hyphenation
static std::string hyphenate(const std::string& word) {
  auto breaks = Hyphenation::breakOffsets(word, false);
  if (breaks.empty()) return word;

  std::string result;
  size_t prev = 0;
  for (const auto& b : breaks) {
    result += word.substr(prev, b.byteOffset - prev);
    result += "-";
    prev = b.byteOffset;
  }
  result += word.substr(prev);
  return result;
}

int main() {
  TestUtils::TestRunner runner("Hyphenation");

  // ============================================
  // HyphenationCommon: isAlphabetic
  // ============================================

  runner.expectTrue(isAlphabetic('a'), "isAlphabetic: lowercase a");
  runner.expectTrue(isAlphabetic('Z'), "isAlphabetic: uppercase Z");
  runner.expectTrue(isAlphabetic(0x00E9), "isAlphabetic: e-acute (Latin Extended)");
  runner.expectTrue(isAlphabetic(0x0410), "isAlphabetic: Cyrillic А");
  runner.expectTrue(isAlphabetic(0x044F), "isAlphabetic: Cyrillic я");
  runner.expectFalse(isAlphabetic('0'), "isAlphabetic: digit 0");
  runner.expectFalse(isAlphabetic(' '), "isAlphabetic: space");
  runner.expectFalse(isAlphabetic('-'), "isAlphabetic: hyphen");
  runner.expectFalse(isAlphabetic(0x4E00), "isAlphabetic: CJK not alphabetic");

  // ============================================
  // HyphenationCommon: toLowerLatin / toLowerCyrillic
  // ============================================

  runner.expectEq(static_cast<uint32_t>('a'), toLowerLatin('A'), "toLowerLatin: A -> a");
  runner.expectEq(static_cast<uint32_t>('z'), toLowerLatin('Z'), "toLowerLatin: Z -> z");
  runner.expectEq(static_cast<uint32_t>('a'), toLowerLatin('a'), "toLowerLatin: a unchanged");
  runner.expectEq(static_cast<uint32_t>(0x00E0), toLowerLatin(0x00C0), "toLowerLatin: À -> à");
  runner.expectEq(static_cast<uint32_t>(0x0153), toLowerLatin(0x0152), "toLowerLatin: Œ -> œ");

  runner.expectEq(static_cast<uint32_t>(0x0430), toLowerCyrillic(0x0410), "toLowerCyrillic: А -> а");
  runner.expectEq(static_cast<uint32_t>(0x044F), toLowerCyrillic(0x042F), "toLowerCyrillic: Я -> я");
  runner.expectEq(static_cast<uint32_t>(0x0451), toLowerCyrillic(0x0401), "toLowerCyrillic: Ё -> ё");

  // ============================================
  // HyphenationCommon: isPunctuation / isExplicitHyphen / isSoftHyphen
  // ============================================

  runner.expectTrue(isPunctuation('.'), "isPunctuation: period");
  runner.expectTrue(isPunctuation(','), "isPunctuation: comma");
  runner.expectTrue(isPunctuation(0x2019), "isPunctuation: right single quote");
  runner.expectFalse(isPunctuation('a'), "isPunctuation: letter a");

  runner.expectTrue(isExplicitHyphen('-'), "isExplicitHyphen: ASCII hyphen");
  runner.expectTrue(isExplicitHyphen(0x00AD), "isExplicitHyphen: soft hyphen");
  runner.expectTrue(isExplicitHyphen(0x2013), "isExplicitHyphen: en dash");
  runner.expectFalse(isExplicitHyphen('a'), "isExplicitHyphen: letter");

  runner.expectTrue(isSoftHyphen(0x00AD), "isSoftHyphen: soft hyphen");
  runner.expectFalse(isSoftHyphen('-'), "isSoftHyphen: ASCII hyphen");

  // ============================================
  // HyphenationCommon: collectCodepoints
  // ============================================

  {
    auto cps = collectCodepoints("abc");
    runner.expectEq(static_cast<size_t>(3), cps.size(), "collectCodepoints: ASCII 3 chars");
    runner.expectEq(static_cast<uint32_t>('a'), cps[0].value, "collectCodepoints: first char");
    runner.expectEq(static_cast<size_t>(0), cps[0].byteOffset, "collectCodepoints: first offset");
    runner.expectEq(static_cast<size_t>(1), cps[1].byteOffset, "collectCodepoints: second offset");
  }

  {
    // "café" = 63 61 66 c3a9 = 5 bytes, 4 codepoints
    auto cps = collectCodepoints("caf\xC3\xA9");
    runner.expectEq(static_cast<size_t>(4), cps.size(), "collectCodepoints: café = 4 cps");
    runner.expectEq(static_cast<uint32_t>(0xE9), cps[3].value, "collectCodepoints: é value");
    runner.expectEq(static_cast<size_t>(3), cps[3].byteOffset, "collectCodepoints: é at byte 3");
  }

  {
    auto cps = collectCodepoints("");
    runner.expectTrue(cps.empty(), "collectCodepoints: empty string");
  }

  // ============================================
  // HyphenationCommon: trimSurroundingPunctuationAndFootnote
  // ============================================

  {
    auto cps = collectCodepoints("\"hello\"");
    trimSurroundingPunctuationAndFootnote(cps);
    runner.expectEq(static_cast<size_t>(5), cps.size(), "trimPunct: quotes removed");
    runner.expectEq(static_cast<uint32_t>('h'), cps[0].value, "trimPunct: starts with h");
    runner.expectEq(static_cast<uint32_t>('o'), cps[4].value, "trimPunct: ends with o");
  }

  {
    auto cps = collectCodepoints("word[12]");
    trimSurroundingPunctuationAndFootnote(cps);
    runner.expectEq(static_cast<size_t>(4), cps.size(), "trimFootnote: [12] removed");
  }

  {
    auto cps = collectCodepoints("hello");
    trimSurroundingPunctuationAndFootnote(cps);
    runner.expectEq(static_cast<size_t>(5), cps.size(), "trimPunct: no change for clean word");
  }

  // ============================================
  // LanguageRegistry
  // ============================================

  {
    auto entries = getLanguageEntries();
    runner.expectTrue(entries.size >= 7, "registry: at least 7 languages");

    bool foundEn = false, foundFr = false, foundDe = false, foundRu = false;
    for (const auto& e : entries) {
      if (std::string(e.primaryTag) == "en") foundEn = true;
      if (std::string(e.primaryTag) == "fr") foundFr = true;
      if (std::string(e.primaryTag) == "de") foundDe = true;
      if (std::string(e.primaryTag) == "ru") foundRu = true;
    }
    runner.expectTrue(foundEn, "registry: English registered");
    runner.expectTrue(foundFr, "registry: French registered");
    runner.expectTrue(foundDe, "registry: German registered");
    runner.expectTrue(foundRu, "registry: Russian registered");
  }

  {
    auto* hyp = getLanguageHyphenatorForPrimaryTag("en");
    runner.expectTrue(hyp != nullptr, "registry: English hyphenator found");
  }

  {
    auto* hyp = getLanguageHyphenatorForPrimaryTag("xx");
    runner.expectTrue(hyp == nullptr, "registry: unknown lang returns nullptr");
  }

  // ============================================
  // Hyphenation::setLanguage + breakOffsets (English)
  // ============================================

  Hyphenation::setLanguage("en");

  // Known English hyphenation: "hyphenation" -> "hy-phen-ation" or similar
  {
    auto breaks = Hyphenation::breakOffsets("hyphenation", false);
    runner.expectTrue(!breaks.empty(), "en: 'hyphenation' has break points");

    // All breaks should require inserted hyphen (not explicit hyphens)
    for (const auto& b : breaks) {
      runner.expectTrue(b.requiresInsertedHyphen, "en: break requires inserted hyphen");
    }

    // Verify break offsets are in ascending order
    for (size_t i = 1; i < breaks.size(); ++i) {
      runner.expectTrue(breaks[i].byteOffset > breaks[i - 1].byteOffset,
                        "en: break offsets ascending");
    }
  }

  // Short words should not be hyphenated (min prefix/suffix = 3 for English)
  {
    auto breaks = Hyphenation::breakOffsets("the", false);
    runner.expectTrue(breaks.empty(), "en: 'the' too short to hyphenate");
  }

  {
    auto breaks = Hyphenation::breakOffsets("go", false);
    runner.expectTrue(breaks.empty(), "en: 'go' too short to hyphenate");
  }

  // Empty string
  {
    auto breaks = Hyphenation::breakOffsets("", false);
    runner.expectTrue(breaks.empty(), "en: empty string no breaks");
  }

  // Single character
  {
    auto breaks = Hyphenation::breakOffsets("a", false);
    runner.expectTrue(breaks.empty(), "en: single char no breaks");
  }

  // Word with explicit soft hyphen should use explicit breaks
  {
    auto breaks = Hyphenation::breakOffsets("auto\xC2\xAD" "matic", false);
    runner.expectEq(static_cast<size_t>(1), breaks.size(), "en: soft hyphen gives 1 break");
    runner.expectEq(static_cast<size_t>(6), breaks[0].byteOffset, "en: soft hyphen break at byte 6");
    runner.expectTrue(breaks[0].requiresInsertedHyphen, "en: soft hyphen needs inserted hyphen");
  }

  // Word with explicit ASCII hyphen
  {
    auto breaks = Hyphenation::breakOffsets("self-aware", false);
    runner.expectEq(static_cast<size_t>(1), breaks.size(), "en: hard hyphen gives 1 break");
    runner.expectEq(static_cast<size_t>(5), breaks[0].byteOffset, "en: hard hyphen break after hyphen");
    runner.expectFalse(breaks[0].requiresInsertedHyphen, "en: hard hyphen doesn't need inserted hyphen");
  }

  // Verify hyphenation result makes sense (splits produce valid parts)
  {
    std::string word = "international";
    auto breaks = Hyphenation::breakOffsets(word, false);
    runner.expectTrue(!breaks.empty(), "en: 'international' has breaks");

    // Verify all offsets are within word bounds
    for (const auto& b : breaks) {
      runner.expectTrue(b.byteOffset > 0 && b.byteOffset < word.size(),
                        "en: break offset within word bounds");
    }
  }

  // ============================================
  // Fallback mode (includeFallback=true)
  // ============================================

  {
    // A non-English word with no dictionary pattern should get fallback breaks
    Hyphenation::setLanguage("en");
    auto breaks = Hyphenation::breakOffsets("zzzzzzzzz", true);
    runner.expectTrue(!breaks.empty(), "fallback: unknown word gets fallback breaks");
  }

  {
    // Fallback breaks should respect min prefix/suffix
    Hyphenation::setLanguage("en");
    auto breaks = Hyphenation::breakOffsets("abcde", true);
    runner.expectTrue(breaks.empty(), "fallback: 5-char word with min prefix=3, min suffix=3 has no breaks");
    auto breaks7 = Hyphenation::breakOffsets("abcdefg", true);
    for (const auto& b : breaks7) {
      runner.expectTrue(b.byteOffset >= 3, "fallback: respects min prefix");
      runner.expectTrue(b.byteOffset <= 4, "fallback: respects min suffix");
    }
  }

  // ============================================
  // Language switching
  // ============================================

  // German hyphenation
  {
    Hyphenation::setLanguage("de");
    auto breaks = Hyphenation::breakOffsets("Donaudampfschifffahrt", false);
    runner.expectTrue(!breaks.empty(), "de: compound word has breaks");
  }

  // French hyphenation
  {
    Hyphenation::setLanguage("fr");
    auto breaks = Hyphenation::breakOffsets("international", false);
    runner.expectTrue(!breaks.empty(), "fr: 'international' has breaks");
  }

  // Russian hyphenation
  {
    Hyphenation::setLanguage("ru");
    // "программирование" (programming)
    auto breaks = Hyphenation::breakOffsets(
        "\xD0\xBF\xD1\x80\xD0\xBE\xD0\xB3\xD1\x80\xD0\xB0\xD0\xBC\xD0\xBC"
        "\xD0\xB8\xD1\x80\xD0\xBE\xD0\xB2\xD0\xB0\xD0\xBD\xD0\xB8\xD0\xB5",
        false);
    runner.expectTrue(!breaks.empty(), "ru: long Russian word has breaks");
  }

  // Empty language tag resets to no hyphenator
  {
    Hyphenation::setLanguage("");
    auto breaks = Hyphenation::breakOffsets("hyphenation", false);
    runner.expectTrue(breaks.empty(), "no lang: dictionary hyphenation disabled");
  }

  // Language tag with region subtag (en-US should resolve to en)
  {
    Hyphenation::setLanguage("en-US");
    auto breaks = Hyphenation::breakOffsets("hyphenation", false);
    runner.expectTrue(!breaks.empty(), "en-US: resolves to English hyphenator");
  }

  // Uppercase language tag
  {
    Hyphenation::setLanguage("EN");
    auto breaks = Hyphenation::breakOffsets("hyphenation", false);
    runner.expectTrue(!breaks.empty(), "EN: case-insensitive language tag");
  }

  // ============================================
  // Non-alphabetic words
  // ============================================

  {
    Hyphenation::setLanguage("en");
    auto breaks = Hyphenation::breakOffsets("12345", false);
    runner.expectTrue(breaks.empty(), "en: digits not hyphenated");
  }

  {
    auto breaks = Hyphenation::breakOffsets("---", false);
    runner.expectTrue(breaks.empty(), "en: punctuation only not hyphenated");
  }

  // Reset to English for consistent state
  Hyphenation::setLanguage("en");

  return runner.allPassed() ? 0 : 1;
}
