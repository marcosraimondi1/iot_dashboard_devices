/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>

int main(void) {
  printf("Hello World from Zephyr in %s\n", CONFIG_BOARD_TARGET);

  return 0;
}
