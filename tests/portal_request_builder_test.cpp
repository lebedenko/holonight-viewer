#include "portal_request_builder.h"

#include <QDBusMetaType>
#include <QRegularExpression>
#include <QSet>

#include <gtest/gtest.h>

TEST(PortalRequestBuilder, FiltersFollowNameFilters) {
  const auto filters = PortalRequest::filters({"Images (*.jpg *.png)", "All files (*)"});
  const QList<PortalFileFilter> expected{
      {.label = "Images", .patterns = {{.type = 0, .pattern = "*.jpg"}, {.type = 0, .pattern = "*.png"}}},
      {.label = "All files", .patterns = {{.type = 0, .pattern = "*"}}},
  };
  EXPECT_EQ(filters, expected);
  const auto bare = PortalRequest::filters({"*.webp  *.gif", "Photos (*.jpeg)"});
  ASSERT_EQ(bare.size(), 2);
  EXPECT_EQ(bare[0].label, "*.webp  *.gif");
  EXPECT_EQ(bare[0].patterns.size(), 2);
  EXPECT_EQ(bare[1].label, "Photos");
  EXPECT_TRUE(PortalRequest::filters({}).isEmpty());
  EXPECT_TRUE(PortalRequest::filters({"", "   ", "Empty ()"}).isEmpty());
  const auto unlabelled = PortalRequest::filters({"(*.tif)"});
  ASSERT_EQ(unlabelled.size(), 1);
  EXPECT_EQ(unlabelled[0].label, "*.tif");
}

TEST(PortalRequestBuilder, FiltersMarshalAsPortalSignatures) {
  PortalRequest::registerMetaTypes();
  EXPECT_STREQ(QDBusMetaType::typeToSignature(QMetaType::fromType<QList<PortalFileFilter>>()), "a(sa(us))");
  EXPECT_STREQ(QDBusMetaType::typeToSignature(QMetaType::fromType<PortalFileFilter>()), "(sa(us))");
}

TEST(PortalRequestBuilder, CurrentFolderIsNulTerminatedLocalDirectory) {
  const auto folder = PortalRequest::currentFolder("/tmp/test/image.jpg");
  EXPECT_EQ(folder, QByteArray("/tmp/test\0", 10));
  EXPECT_EQ(folder.back(), '\0');
  EXPECT_EQ(PortalRequest::currentFolder("/tmp/café dir/a b.png"), QByteArray("/tmp/caf\xc3\xa9 dir\0", 15));
  EXPECT_EQ(PortalRequest::currentFolder("/image.jpg"), QByteArray("/\0", 2));
  EXPECT_TRUE(PortalRequest::currentFolder({}).isEmpty());
}

TEST(PortalRequestBuilder, ParentWindowAndRequestPath) {
  EXPECT_EQ(PortalRequest::parentWindow("abc"), "wayland:abc");
  EXPECT_EQ(PortalRequest::parentWindow({}), "");
  EXPECT_EQ(PortalRequest::senderPathElement(":1.42"), "1_42");
  EXPECT_EQ(PortalRequest::senderPathElement(""), "");
  EXPECT_EQ(PortalRequest::requestPath(":1.42", "hn_token"), "/org/freedesktop/portal/desktop/request/1_42/hn_token");
}

TEST(PortalRequestBuilder, HandleTokensAreDistinctPathElements) {
  const QRegularExpression element("^[A-Za-z0-9_]+$");
  QSet<QString> tokens;
  for (int i = 0; i < 64; ++i) {
    const auto token = PortalRequest::newHandleToken();
    EXPECT_TRUE(element.match(token).hasMatch()) << token.toStdString();
    tokens.insert(token);
  }
  EXPECT_EQ(tokens.size(), 64);
}
