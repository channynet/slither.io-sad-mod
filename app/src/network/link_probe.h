#ifndef LINK_PROBE_H
#define LINK_PROBE_H
#include <stdbool.h>
// Background measurements of the local link, never on the game thread:
// round trip to the home router and Wi-Fi scan / roam events. They tell a
// local Wi-Fi delay apart from one further along the route.
typedef struct {
  float gateway_ms;        // -1 when unknown or no reply
  int signal;              // Wi-Fi signal 0..100; -1 no adapter, -2 not connected (wired), -3 query refused
  unsigned scans,roams;    // totals since start
  unsigned tuned;          // bit 0: streaming mode on, bit 1: background scan off
} link_sample;
void link_probe_init(void);
void link_probe_destroy(void);
void link_probe_read(link_sample* out);
// While true, ask the Wi-Fi adapter to stop background scanning (the usual
// source of periodic latency bursts). Restored when false and on exit.
void link_probe_low_latency(bool wanted);
#endif
