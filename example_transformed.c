#include <stdio.h>

int add_twice(int x) {
  int s = 0;
  for (int i = 0; i < 4; i++) {
    s = s + x;
  }
  return s;
}

int main() {
  int a = 10;
  int b = add_twice(a);
  int c = a << 2;
  int d = a >> 1;
  printf("%d %d\n", b, c + d);
  return 0;
}

