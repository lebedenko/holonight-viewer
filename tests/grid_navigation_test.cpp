#include "grid_navigation.h"

#include <gtest/gtest.h>

namespace {
using Move = GridNavigation::Move;

// The SPEC examples use one-based cells; these helpers keep that numbering.
int cell(int cellNumber, Move move, int count, int columns, int visibleRows = 4) {
  return GridNavigation::target(cellNumber - 1, move, count, columns, visibleRows) + 1;
}
}  // namespace

TEST(GridNavigation, HorizontalMovesFlowAcrossRowsWithoutWrapping) {
  EXPECT_EQ(cell(1, Move::Previous, 10, 3), 1);
  EXPECT_EQ(cell(1, Move::Next, 10, 3), 2);
  EXPECT_EQ(cell(3, Move::Next, 10, 3), 4);
  EXPECT_EQ(cell(4, Move::Previous, 10, 3), 3);
  EXPECT_EQ(cell(10, Move::Next, 10, 3), 10);
}

TEST(GridNavigation, RowMovesClampToTheLastItemAndStopAtTheEnds) {
  EXPECT_EQ(cell(7, Move::RowDown, 7, 3), 7);
  EXPECT_EQ(cell(3, Move::RowDown, 7, 3), 6);
  EXPECT_EQ(cell(6, Move::RowDown, 7, 3), 7);
  EXPECT_EQ(cell(2, Move::RowUp, 7, 3), 2);
  EXPECT_EQ(cell(5, Move::RowUp, 7, 3), 2);
  EXPECT_EQ(cell(5, Move::RowDown, 7, 3), 7);
}

TEST(GridNavigation, PageMovesUseHalfTheVisibleRowsAndClamp) {
  EXPECT_EQ(cell(1, Move::PageDown, 30, 3), 7);
  EXPECT_EQ(cell(25, Move::PageDown, 30, 3), 30);
  EXPECT_EQ(cell(14, Move::PageUp, 30, 3), 8);
  EXPECT_EQ(cell(5, Move::PageUp, 30, 3), 1);
  EXPECT_EQ(cell(1, Move::PageDown, 30, 3, 1), 4);
  EXPECT_EQ(cell(14, Move::PageUp, 30, 3, 1), 11);
  EXPECT_EQ(cell(1, Move::PageDown, 30, 3, 5), 7);
}

TEST(GridNavigation, DegenerateInputsStayInRange) {
  EXPECT_EQ(GridNavigation::target(0, Move::Next, 0, 3, 4), -1);
  EXPECT_EQ(GridNavigation::target(0, Move::Next, -5, 3, 4), -1);
  // columns below 1 behave as one column.
  EXPECT_EQ(GridNavigation::target(2, Move::RowDown, 5, 0, 4), 3);
  EXPECT_EQ(GridNavigation::target(2, Move::RowUp, 5, -3, 4), 1);
  // visibleRows below 0 behaves as 0, and a page still moves one row.
  EXPECT_EQ(GridNavigation::target(0, Move::PageDown, 30, 3, -1), 3);
  EXPECT_EQ(GridNavigation::target(0, Move::PageDown, 30, 3, 0), 3);
  // A current index outside the list counts as the first item.
  EXPECT_EQ(GridNavigation::target(-1, Move::Next, 5, 3, 4), 1);
  EXPECT_EQ(GridNavigation::target(9, Move::Previous, 5, 3, 4), 0);
  EXPECT_EQ(GridNavigation::target(0, Move::Next, 1, 3, 4), 0);
  EXPECT_EQ(GridNavigation::target(0, Move::RowDown, 1, 3, 4), 0);
}

TEST(GridNavigation, EveryMoveStaysInRangeForEverySmallGrid) {
  for (int count = 1; count <= 12; ++count) {
    for (int columns = 1; columns <= 5; ++columns) {
      for (int current = 0; current < count; ++current) {
        for (const Move move : {Move::Previous, Move::Next, Move::RowUp, Move::RowDown, Move::PageUp, Move::PageDown}) {
          const int result = GridNavigation::target(current, move, count, columns, 4);
          ASSERT_GE(result, 0);
          ASSERT_LT(result, count);
        }
      }
    }
  }
}

TEST(GridNavigation, FirstAndLastReachTheBoundariesAndHandleEmptyGrids) {
  EXPECT_EQ(GridNavigation::target(5, Move::First, 20, 3, 4), 0);
  EXPECT_EQ(GridNavigation::target(5, Move::Last, 20, 3, 4), 19);
  EXPECT_EQ(GridNavigation::target(-1, Move::Last, 20, 0, 0), 19);
  EXPECT_EQ(GridNavigation::target(0, Move::First, 0, 3, 4), -1);
  EXPECT_EQ(GridNavigation::target(0, Move::Last, 0, 3, 4), -1);
}
