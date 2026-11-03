package main

import "testing"

func TestStableStatus(t *testing.T) {
	for _, states := range [][]string{{"active", "connection_failed", "connecting"}, {"active", "connection_failed", "active"}, {"connected", "connection_failed", "active"}} {
		s := summarize(states)
		if s.State != "connection_failed" || s.Total != 3 {
			t.Fatalf("failure hidden: %+v", s)
		}
	}
	if s := summarize([]string{"active", "active"}); s.State != "active" || s.Active != 2 {
		t.Fatal(s)
	}
}
