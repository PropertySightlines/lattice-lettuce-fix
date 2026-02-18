
#include <stdbool.h>
#include <stdio.h>

#define N 9

bool can_place(int board[N][N], int row, int col, int num) {
  for (int x = 0; x < N; x++) {
    if (board[row][x] == num || board[x][col] == num) {
      return false;
    }
  }

  int startRow = row - row % 3;
  int startCol = col - col % 3;

  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      if (board[i + startRow][j + startCol] == num) {
        return false;
      }
    }
  }

  return true;
}

bool solve_sudoku(int board[N][N], int row, int col) {
  if (row == N - 1 && col == N) {
    return true;
  }

  if (col == N) {
    row++;
    col = 0;
  }

  if (board[row][col] != 0) {
    return solve_sudoku(board, row, col + 1);
  }

  for (int num = 1; num <= 9; num++) {
    if (can_place(board, row, col, num)) {
      board[row][col] = num;
      if (solve_sudoku(board, row, col + 1)) {
        return true;
      }
    }
    board[row][col] = 0;
  }

  return false;
}

int main() {
  int total_solved = 0;

  for (int k = 0; k < 600; k++) {
    int board[N][N] = {{3, 0, 6, 5, 0, 8, 4, 0, 0}, {5, 2, 0, 0, 0, 0, 0, 0, 0},
                       {0, 8, 7, 0, 0, 0, 0, 3, 1}, {0, 0, 3, 0, 1, 0, 0, 8, 0},
                       {9, 0, 0, 8, 6, 3, 0, 0, 5}, {0, 5, 0, 0, 9, 0, 6, 0, 0},
                       {1, 3, 0, 0, 0, 0, 2, 5, 0}, {0, 0, 0, 0, 0, 0, 0, 7, 4},
                       {0, 0, 5, 2, 0, 6, 3, 0, 0}};

    if (solve_sudoku(board, 0, 0)) {
      total_solved++;
    }
  }

  return total_solved != 600;
}
