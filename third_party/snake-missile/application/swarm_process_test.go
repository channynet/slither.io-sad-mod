package application

import (
	"context"
	"encoding/json"
	"io"
	"net/http"
	"net/http/httptest"
	"os"
	"os/exec"
	"strings"
	"testing"
	"time"
)

func TestWorkerReportsConnectionFailure(t *testing.T) {
	worker := os.Getenv("SLITHER_MISSILE_WORKER_TEST")
	if worker == "" {
		t.Skip("worker executable not supplied")
	}
	s := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		http.Error(w, "test rejection", http.StatusForbidden)
	}))
	defer s.Close()
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	cmd := exec.CommandContext(ctx, worker)
	in, err := cmd.StdinPipe()
	if err != nil {
		t.Fatal(err)
	}
	out, err := cmd.StdoutPipe()
	if err != nil {
		t.Fatal(err)
	}
	if err = cmd.Start(); err != nil {
		t.Fatal(err)
	}
	defer func() { in.Close(); cmd.Process.Kill(); cmd.Wait() }()
	if err = json.NewEncoder(in).Encode(map[string]any{"command": "start", "server": s.URL[7:], "count": 1}); err != nil {
		t.Fatal(err)
	}
	decoder := json.NewDecoder(out)
	for {
		var status struct{ State, Detail string }
		if err = decoder.Decode(&status); err != nil {
			t.Fatal(err)
		}
		if status.State == "connection_failed" {
			if !strings.Contains(status.Detail, "bad handshake") {
				t.Fatalf("failure detail lost: %+v", status)
			}
			return
		}
	}
}

func TestWorkerModesAndWatchdog(t *testing.T) {
	worker := os.Getenv("SLITHER_MISSILE_WORKER_TEST")
	if worker == "" {
		t.Skip("worker executable not supplied")
	}
	s := newTestServer(t, true, 31)
	ctx, cancel := context.WithTimeout(context.Background(), 15*time.Second)
	defer cancel()
	cmd := exec.CommandContext(ctx, worker)
	cmd.Dir = t.TempDir()
	cmd.Stdout = io.Discard
	cmd.Stderr = io.Discard
	in, err := cmd.StdinPipe()
	if err != nil {
		t.Fatal(err)
	}
	if err = cmd.Start(); err != nil {
		t.Fatal(err)
	}
	defer func() {
		in.Close()
		if cmd.ProcessState == nil {
			cmd.Process.Kill()
			cmd.Wait()
		}
	}()
	enc := json.NewEncoder(in)
	if err = enc.Encode(map[string]any{"command": "start", "server": s.server.URL[7:], "count": 1, "x": 2000, "y": 1000}); err != nil {
		t.Fatal(err)
	}
	waitAngle(t, s, 0)
	enc.Encode(map[string]any{"command": "control", "mode": 2, "angle": 1.57079632679, "x": 2000, "y": 1000})
	waitAngle(t, s, 63)
	enc.Encode(map[string]any{"command": "control", "mode": 3, "angle": 0, "x": 1000, "y": 1000})
	waitAngle(t, s, 0)
	enc.Encode(map[string]any{"command": "control", "mode": 4, "angle": 0, "x": 400, "y": 1000})
	waitAngle(t, s, 63)
	// Keep stdin open but stop sending commands. The worker must exit itself.
	if err = cmd.Wait(); err != nil {
		t.Fatalf("watchdog exit: %v", err)
	}
	waitFor(t, func() bool { return s.live.Load() == 0 })
}
