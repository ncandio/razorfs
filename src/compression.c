/**
 * Lightweight Compression Implementation - RAZORFS Phase 8
 */

#define _GNU_SOURCE
#include "compression.h"
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

/* Global stats */
static struct compression_stats g_stats = {0};

