#include "thumbnail_cache.h"

#include <gtest/gtest.h>

namespace {
ThumbnailKey key(int number, int box = 256, int generation = 0) {
  return ThumbnailKey{
      .path = QStringLiteral("/photos/%1.png").arg(number), .box_pixels = box, .generation = generation};
}

QImage image(int side = 4) {
  QImage result(side, side, QImage::Format_ARGB32_Premultiplied);
  result.fill(Qt::red);
  return result;
}
}  // namespace

TEST(ThumbnailCache, HoldsAtMostThousandEntriesByDefault) {
  ThumbnailCache cache;
  for (int i = 0; i < 1200; ++i) {
    cache.insert(key(i), image());
    ASSERT_LE(cache.count(), 1000);
  }
  EXPECT_EQ(cache.count(), 1000);
  EXPECT_FALSE(cache.find(key(199)).has_value());
  EXPECT_TRUE(cache.find(key(200)).has_value());
  EXPECT_TRUE(cache.find(key(1199)).has_value());
}

TEST(ThumbnailCache, EvictsLeastRecentlyUsedFirstAndFindPromotes) {
  ThumbnailCache cache(3, kThumbnailCacheBytes);
  cache.insert(key(1), image());
  cache.insert(key(2), image());
  cache.insert(key(3), image());
  ASSERT_TRUE(cache.find(key(1)).has_value());
  cache.insert(key(4), image());
  EXPECT_FALSE(cache.find(key(2)).has_value());
  EXPECT_TRUE(cache.find(key(1)).has_value());
  EXPECT_TRUE(cache.find(key(3)).has_value());
  EXPECT_TRUE(cache.find(key(4)).has_value());
}

TEST(ThumbnailCache, KeyIncludesBoxAndGeneration) {
  ThumbnailCache cache;
  cache.insert(key(1, 256, 0), image(2));
  EXPECT_TRUE(cache.find(key(1, 256, 0)).has_value());
  EXPECT_FALSE(cache.find(key(1, 320, 0)).has_value());
  EXPECT_FALSE(cache.find(key(1, 256, 1)).has_value());
}

TEST(ThumbnailCache, HitReturnsTheStoredPixels) {
  ThumbnailCache cache;
  cache.insert(key(1), image(8));
  const auto hit = cache.find(key(1));
  ASSERT_TRUE(hit.has_value());
  EXPECT_EQ(hit->size(), QSize(8, 8));
  EXPECT_EQ(hit->pixel(3, 3), QColor(Qt::red).rgba());
}

TEST(ThumbnailCache, ByteLimitBoundsLargeImages) {
  ThumbnailCache cache;
  for (int i = 0; i < 400; ++i) {
    cache.insert(key(i, 512), image(512));
    ASSERT_LE(cache.bytes(), kThumbnailCacheBytes);
  }
  // 512 x 512 ARGB is 1 MiB, so 256 MiB holds 256 of them.
  EXPECT_EQ(cache.count(), 256);
  EXPECT_EQ(cache.bytes(), kThumbnailCacheBytes);
  EXPECT_FALSE(cache.find(key(143, 512)).has_value());
  EXPECT_TRUE(cache.find(key(144, 512)).has_value());
}

TEST(ThumbnailCache, ReinsertReplacesWithoutDoubleCounting) {
  ThumbnailCache cache;
  cache.insert(key(1), image(10));
  cache.insert(key(1), image(20));
  EXPECT_EQ(cache.count(), 1);
  EXPECT_EQ(cache.bytes(), 20 * 20 * 4);
  EXPECT_EQ(cache.find(key(1))->size(), QSize(20, 20));
}

TEST(ThumbnailCache, ImageLargerThanTheByteLimitIsNotKept) {
  ThumbnailCache cache(10, 1000);
  cache.insert(key(1), image(2));
  cache.insert(key(2), image(20));
  EXPECT_EQ(cache.count(), 0);
  EXPECT_EQ(cache.bytes(), 0);
}
