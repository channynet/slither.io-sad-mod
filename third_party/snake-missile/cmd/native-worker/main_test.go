package main

import "testing"

func TestStableStatus(t *testing.T) {
	for _, states := range [][]string{{"active", "connection_failed", "connecting"}, {"active", "connection_failed", "active"}, {"connected", "connection_failed", "active"}} {
		s := summarize(states, nil)
		if s.State != "connection_failed" || s.Total != 3 {
			t.Fatalf("failure hidden: %+v", s)
		}
	}
	if s := summarize([]string{"active", "active"}, nil); s.State != "active" || s.Active != 2 {
		t.Fatal(s)
	}
}

func TestFailureDetailMatchesVisibleState(t *testing.T) {
	s := summarize([]string{"active", "connection_failed", "handshake_rejected"},
		[]string{"stale", "dial refused", "close 1006"})
	if s.State != "handshake_rejected" || s.Detail != "close 1006" {
		t.Fatal(s)
	}
	if s = summarize([]string{"active"}, []string{"old error"}); s.Detail != "" {
		t.Fatal("stale error on healthy connection", s)
	}
	if s = summarize([]string{"respawning"}, nil); s.State != "respawning" || s.Waiting != 1 {
		t.Fatal("death mislabeled as connection attempt", s)
	}
}
