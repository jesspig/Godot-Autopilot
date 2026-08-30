#include "util/bm25_index.hpp"

#include <gtest/gtest.h>

#include <string>

using godot_autopilot::Bm25Index;

namespace {

godot_autopilot::Bm25Index::SearchQuery make_text_query(
    const std::string &text, int max_results = 10) {
  godot_autopilot::Bm25Index::SearchQuery q;
  q.text = text;
  q.max_results = max_results;
  return q;
}

void add_sprite_entries(Bm25Index &idx) {
  idx.add_entry("sprite_frames", "Create sprite frames for animation", "Sprite",
                {"animation", "frames"});
  idx.add_entry("audio_play", "Play audio stream", "Audio", {"sound"});
}

} // namespace

TEST(Bm25IndexTest, HitReturnsMatchingToolFirst) {
  Bm25Index idx;
  add_sprite_entries(idx);
  auto results = idx.search(make_text_query("sprite frames"));
  ASSERT_FALSE(results.empty());
  EXPECT_EQ(results[0].name, "sprite_frames");
}

TEST(Bm25IndexTest, MissReturnsEmpty) {
  Bm25Index idx;
  add_sprite_entries(idx);
  EXPECT_TRUE(idx.search(make_text_query("zebra unicorn")).empty());
}

TEST(Bm25IndexTest, MoreMatchingTokensRankHigher) {
  Bm25Index idx;
  idx.add_entry("banana_doc", "banana apple smoothie", "Food", {});
  idx.add_entry("plain_doc", "banana", "Food", {});
  auto results = idx.search(make_text_query("apple banana"));
  ASSERT_EQ(results.size(), 2u);
  EXPECT_EQ(results[0].name, "banana_doc");
  EXPECT_GT(results[0].score, results[1].score);
}

TEST(Bm25IndexTest, NameSubstringBonusLiftsZeroBm25Hit) {
  Bm25Index idx;
  idx.add_entry("spriteframes", "unrelated text", "Misc", {});
  auto results = idx.search(make_text_query("sprite frames"));
  ASSERT_EQ(results.size(), 1u);
  EXPECT_DOUBLE_EQ(results[0].score, 2.0);
}

TEST(Bm25IndexTest, CaseInsensitiveMatching) {
  Bm25Index idx;
  add_sprite_entries(idx);
  auto results = idx.search(make_text_query("SPRITE FRAMES"));
  ASSERT_FALSE(results.empty());
  EXPECT_EQ(results[0].name, "sprite_frames");
}

TEST(Bm25IndexTest, CategoryFilterRestrictsResults) {
  Bm25Index idx;
  add_sprite_entries(idx);
  Bm25Index::SearchQuery q;
  q.text = "audio";
  q.category = "Audio";
  auto results = idx.search(q);
  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(results[0].name, "audio_play");
}

TEST(Bm25IndexTest, CategoryFilterWithNonMatchingTextReturnsEmpty) {
  Bm25Index idx;
  add_sprite_entries(idx);
  Bm25Index::SearchQuery q;
  q.text = "sprite";
  q.category = "Audio";
  EXPECT_TRUE(idx.search(q).empty());
}

TEST(Bm25IndexTest, EmptyQueryWithCategoryListsCategoryDocs) {
  Bm25Index idx;
  add_sprite_entries(idx);
  Bm25Index::SearchQuery q;
  q.text = "";
  q.category = "Sprite";
  auto results = idx.search(q);
  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(results[0].name, "sprite_frames");
  EXPECT_DOUBLE_EQ(results[0].score, 0.0);
}

TEST(Bm25IndexTest, EmptyQueryWithoutCategoryReturnsEmpty) {
  Bm25Index idx;
  add_sprite_entries(idx);
  EXPECT_TRUE(idx.search(make_text_query("")).empty());
}

TEST(Bm25IndexTest, SymbolOnlyQueryReturnsEmpty) {
  Bm25Index idx;
  add_sprite_entries(idx);
  EXPECT_TRUE(idx.search(make_text_query("!!!")).empty());
}

TEST(Bm25IndexTest, TagsFilterRequiresAllTags) {
  Bm25Index idx;
  add_sprite_entries(idx);
  Bm25Index::SearchQuery q;
  q.text = "sprite";
  q.tags = std::vector<std::string>{"animation", "frames"};
  auto results = idx.search(q);
  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(results[0].name, "sprite_frames");

  q.tags = std::vector<std::string>{"animation", "missing"};
  EXPECT_TRUE(idx.search(q).empty());
}

TEST(Bm25IndexTest, MaxResultsTruncates) {
  Bm25Index idx;
  for (int i = 0; i < 5; ++i) {
    idx.add_entry("t" + std::to_string(i), "common stuff", "Cat", {});
  }
  auto results = idx.search(make_text_query("common", 2));
  ASSERT_EQ(results.size(), 2u);
}

TEST(Bm25IndexTest, ClearEmptiesIndex) {
  Bm25Index idx;
  add_sprite_entries(idx);
  EXPECT_EQ(idx.size(), 2u);
  idx.clear();
  EXPECT_EQ(idx.size(), 0u);
  EXPECT_TRUE(idx.search(make_text_query("sprite")).empty());
}

TEST(Bm25IndexTest, SizeCountsEntries) {
  Bm25Index idx;
  idx.add_entry("a", "desc a", "C", {});
  idx.add_entry("a", "desc a", "C", {});
  idx.add_entry("b", "desc b", "C", {});
  EXPECT_EQ(idx.size(), 3u);
}

TEST(Bm25IndexTest, ChineseTagQueryMatchesEntryWithPositiveScore) {
  Bm25Index idx;
  idx.add_entry("create_animation", "Create animation player node",
                "Animation", {"动画", "补间"});
  idx.add_entry("audio_play", "Play audio stream", "Audio", {"声音"});
  auto results = idx.search(make_text_query("动画"));
  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(results[0].name, "create_animation");
  EXPECT_GT(results[0].score, 0.0);
}

TEST(Bm25IndexTest, ChineseNameQueryRanksExactNameAbovePartialOverlap) {
  Bm25Index idx;
  idx.add_entry("创建动画节点", "misc filler text", "Misc", {});
  idx.add_entry("tween_helper", "创建 misc filler text", "Misc", {});
  auto results = idx.search(make_text_query("创建动画"));
  ASSERT_EQ(results.size(), 2u);
  EXPECT_EQ(results[0].name, "创建动画节点");
  EXPECT_GT(results[0].score, results[1].score);
}

TEST(Bm25IndexTest, SingleCjkCharQueryReturnsEmpty) {
  Bm25Index idx;
  idx.add_entry("动画工具", "misc filler text", "Misc", {});
  EXPECT_TRUE(idx.search(make_text_query("动")).empty());
}

TEST(Bm25IndexTest, MixedAsciiAndCjkQueryHitsAcrossScripts) {
  Bm25Index idx;
  idx.add_entry("play_audio", "Play audio stream", "Audio", {"播放"});
  idx.add_entry("create_sprite", "Create sprite frames", "Sprite", {});
  auto results = idx.search(make_text_query("play 播放"));
  ASSERT_FALSE(results.empty());
  EXPECT_EQ(results[0].name, "play_audio");
  EXPECT_GT(results[0].score, 0.0);
}

TEST(Bm25IndexTest, InvalidUtf8QueryBytesAreIgnored) {
  Bm25Index idx;
  idx.add_entry("audio_play", "Play audio stream", "Audio", {"声音"});
  EXPECT_TRUE(idx.search(make_text_query("\xFF\xC3")).empty());
  auto results = idx.search(make_text_query("\xFF声音"));
  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(results[0].name, "audio_play");
}

TEST(Bm25IndexTest, AsciiEnglishRegressionUnchangedByTokenizerRework) {
  Bm25Index idx;
  add_sprite_entries(idx);
  auto results = idx.search(make_text_query("sprite frames"));
  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(results[0].name, "sprite_frames");
  EXPECT_GT(results[0].score, 0.0);
}
