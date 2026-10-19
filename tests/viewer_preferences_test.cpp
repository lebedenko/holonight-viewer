// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Andrii L <lebeden@gmail.com>
#include "viewer_preferences.h"

#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QTest>

#include <gtest/gtest.h>

namespace {
void writeConfig(const QString& path, const QByteArray& contents) {
  ASSERT_TRUE(QDir().mkpath(QFileInfo(path).absolutePath()));
  QFile file(path);
  ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
  ASSERT_EQ(file.write(contents), contents.size());
}
using Source = ViewerPreferences::Source;
}  // namespace

TEST(ViewerPreferences, DefaultsAndXdgFallbackDoNotCreateFiles) {
  QTemporaryDir directory;
  ASSERT_TRUE(directory.isValid());
  const auto fallback = directory.filePath(".config/holonight-viewer");
  EXPECT_EQ(ViewerPreferences::configurationDirectory({}, directory.path()), fallback);
  EXPECT_EQ(ViewerPreferences::configurationDirectory(directory.filePath("xdg"), "/unused"),
            directory.filePath("xdg/holonight-viewer"));
  ViewerPreferences preferences({}, directory.path(), {});
  EXPECT_TRUE(preferences.showTitle());
  EXPECT_TRUE(preferences.showCloseButton());
  EXPECT_EQ(preferences.titleSource(), Source::Default);
  EXPECT_EQ(preferences.closeSource(), Source::Default);
  EXPECT_FALSE(QFileInfo::exists(fallback));
  writeConfig(fallback + "/config.toml", "[header]\nshow_title = false\n");
  ViewerPreferences fromHome({}, directory.path(), {});
  EXPECT_FALSE(fromHome.showTitle());
  EXPECT_EQ(fromHome.titleSource(), Source::Base);
}

TEST(ViewerPreferences, CombinedDesktopPriorityAndIndividualInheritance) {
  QTemporaryDir directory;
  const auto config = directory.filePath("holonight-viewer/");
  writeConfig(config + "config.toml", "[header]\nshow_title = false\nshow_close_button = false\n");
  writeConfig(config + "config-holonight.toml", "[header]\nshow_title = true\n");
  writeConfig(config + "config-hyprland.toml", "[header]\nshow_close_button = true\n");
  ViewerPreferences preferences(directory.path(), {}, ":HoloNight-Hyprland::HoloNight:Hyprland:");
  EXPECT_TRUE(preferences.showTitle());
  EXPECT_FALSE(preferences.showCloseButton());
  EXPECT_EQ(preferences.titleSource(), Source::Override);
  EXPECT_EQ(preferences.closeSource(), Source::Base);
  EXPECT_EQ(preferences.selectedOverride(), config + "config-holonight.toml");
}

TEST(ViewerPreferences, InvalidNamesValuesAndFilesNeverSelectALaterOverride) {
  QTemporaryDir directory;
  const auto config = directory.filePath("holonight-viewer/");
  writeConfig(config + "config.toml",
              "unknown = 3\n[header]\nshow_title = false\nshow_close_button = 'bad'\nextra = true\n");
  writeConfig(config + "config-first.toml", "[header]\nshow_title = true\ninvalid syntax\n");
  writeConfig(config + "config-later.toml", "[header]\nshow_title = true\nshow_close_button = false\n");
  ViewerPreferences preferences(directory.path(), {}, "../bad:bad.name:非:First:Later");
  EXPECT_FALSE(preferences.showTitle());
  EXPECT_TRUE(preferences.showCloseButton());
  EXPECT_EQ(preferences.titleSource(), Source::Base);
  EXPECT_EQ(preferences.closeSource(), Source::Default);
  EXPECT_EQ(preferences.selectedOverride(), config + "config-first.toml");
  // A directory at a selected file path is also invalid, and still blocks later candidates.
  ASSERT_TRUE(QFile::remove(config + "config-first.toml"));
  ASSERT_TRUE(QDir().mkdir(config + "config-first.toml"));
  ViewerPreferences unreadable(directory.path(), {}, "First:Later");
  EXPECT_FALSE(unreadable.showTitle());
  EXPECT_TRUE(unreadable.showCloseButton());
  EXPECT_EQ(unreadable.selectedOverride(), config + "config-first.toml");
}

TEST(ViewerPreferences, InvalidOverrideValuesInheritPerKeyAndInvalidHeaderIsIgnored) {
  QTemporaryDir directory;
  const auto config = directory.filePath("holonight-viewer/");
  writeConfig(config + "config.toml", "[header]\nshow_title = false\nshow_close_button = false\n");
  writeConfig(config + "config-first.toml", "[header]\nshow_title = 1\nshow_close_button = true\n");
  ViewerPreferences preferences(directory.path(), {}, "First");
  EXPECT_FALSE(preferences.showTitle());
  EXPECT_TRUE(preferences.showCloseButton());
  EXPECT_EQ(preferences.titleSource(), Source::Base);
  EXPECT_EQ(preferences.closeSource(), Source::Override);
  writeConfig(config + "config-first.toml", "header = false\n");
  ASSERT_TRUE(QTest::qWaitFor([&] { return !preferences.showCloseButton(); }));
  EXPECT_EQ(preferences.closeSource(), Source::Base);
  writeConfig(config + "config.toml", "malformed = [");
  ASSERT_TRUE(QTest::qWaitFor([&] { return preferences.showTitle() && preferences.showCloseButton(); }));
  EXPECT_EQ(preferences.titleSource(), Source::Default);
  EXPECT_EQ(preferences.closeSource(), Source::Default);
}

TEST(ViewerPreferences, WatchesCreationEditsAtomicReplacementDeletionAndDirectoryRecreation) {
  QTemporaryDir directory;
  const auto root = directory.filePath("absent/nested");
  const auto config = root + "/holonight-viewer/";
  ViewerPreferences preferences(root, {}, "First:Later");
  writeConfig(config + "config.toml", "[header]\nshow_title = false\n");
  ASSERT_TRUE(QTest::qWaitFor([&] { return !preferences.showTitle(); }));
  writeConfig(config + "config.toml", "[header]\nshow_title = true\nshow_close_button = false\n");
  ASSERT_TRUE(QTest::qWaitFor([&] { return preferences.showTitle() && !preferences.showCloseButton(); }));
  QSaveFile replacement(config + "config.toml");
  ASSERT_TRUE(replacement.open(QIODevice::WriteOnly));
  ASSERT_GT(replacement.write("[header]\nshow_title = false\n"), 0);
  ASSERT_TRUE(replacement.commit());
  ASSERT_TRUE(QTest::qWaitFor([&] { return !preferences.showTitle() && preferences.showCloseButton(); }));
  writeConfig(config + "config-later.toml", "[header]\nshow_title = true\n");
  ASSERT_TRUE(QTest::qWaitFor([&] { return preferences.showTitle(); }));
  writeConfig(config + "config-first.toml", "[header]\nshow_close_button = false\n");
  ASSERT_TRUE(QTest::qWaitFor([&] { return !preferences.showTitle() && !preferences.showCloseButton(); }));
  writeConfig(config + "config-first.toml", "malformed = [");
  ASSERT_TRUE(QTest::qWaitFor([&] { return !preferences.showTitle() && preferences.showCloseButton(); }));
  ASSERT_TRUE(QFile::remove(config + "config-first.toml"));
  ASSERT_TRUE(QTest::qWaitFor([&] { return preferences.showTitle(); }));
  // Move the whole tree away, then recreate it at the original location.
  ASSERT_TRUE(QDir().rename(root, root + "-old"));
  ASSERT_TRUE(QTest::qWaitFor([&] { return preferences.titleSource() == Source::Default; }));
  writeConfig(config + "config.toml", "[header]\nshow_title = false\n");
  ASSERT_TRUE(QTest::qWaitFor([&] { return !preferences.showTitle(); }));
}
