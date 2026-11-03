package application

import (
	"context"
	"io"
	"net/http"
	"net/http/httptest"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"sync/atomic"
	"testing"
	"time"

	"github.com/fasthttp/websocket"
)

type testServer struct {
	server      *httptest.Server
	live        atomic.Int32
	ids         atomic.Int32
	angles      chan byte
	east, south atomic.Int32
}

func newTestServer(t *testing.T, modern bool, versions ...byte) *testServer {
	t.Helper()
	s := &testServer{angles: make(chan byte, 512)}
	s.server = httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		up := websocket.Upgrader{CheckOrigin: func(*http.Request) bool { return true }}
		c, err := up.Upgrade(w, r, nil)
		if err != nil {
			t.Error(err)
			return
		}
		defer c.Close()
		s.live.Add(1)
		defer s.live.Add(-1)
		read := func() []byte {
			_, p, err := c.ReadMessage()
			if err != nil {
				return nil
			}
			return p
		}
		write := func(p []byte) { _ = c.WriteMessage(websocket.BinaryMessage, p) }
		if p := read(); len(p) != 1 || p[0] != 99 {
			t.Errorf("wrong HELLO: %v", p)
			return
		}
		challenge := make([]byte, 65)
		for i := range challenge {
			challenge[i] = 'a'
		}
		challenge[0] = 0
		challenge[1] = 0
		challenge[2] = '6'
		if modern {
			challenge = make([]byte, 57) // private server's shorter challenge is valid too
			for i := range challenge {
				challenge[i] = 'a'
			}
			challenge[0] = '6'
		}
		write(challenge)
		secret := read()
		expected := 24
		if modern {
			expected = 27
		}
		if len(secret) != expected {
			t.Errorf("secret size %d, want %d", len(secret), expected)
			return
		}
		spawn := read()
		if len(spawn) < 4 || spawn[0] != 's' {
			t.Errorf("missing spawn: %v", spawn)
			return
		}
		if modern && (len(spawn) < 28 || spawn[1] != 30 || !strings.HasPrefix(string(spawn[26:]), "Missile_")) {
			t.Errorf("invalid modern spawn: %v", spawn)
			return
		}
		if !modern && (spawn[1] != 10 || !strings.HasPrefix(string(spawn[4:]), "Missile_")) {
			t.Errorf("invalid original spawn: %v", spawn)
			return
		}
		id := int(s.ids.Add(1))
		snake := make([]byte, 31)
		snake[2] = 's'
		snake[3] = byte(id >> 8)
		snake[4] = byte(id)
		for _, at := range []int{18, 21} {
			snake[at] = 0
			snake[at+1] = 19
			snake[at+2] = 136
		} // (1000,1000)*5
		if modern {
			init := make([]byte, 25)
			init[0] = 'a'
			init[23] = 14
			if len(versions) > 0 {
				init[23] = versions[0]
			}
			init[24] = 42
			body := append([]byte{0, byte(len(init))}, init...)
			body = append(body, 0, byte(len(snake)-2))
			body = append(body, snake[2:]...)
			write(body)
		} else {
			write([]byte{0, 0, 'a'})
			write(snake)
		}
		east, south := false, false
		for {
			p := read()
			if len(p) == 0 {
				return
			}
			if p[0] == 251 {
				if modern {
					write([]byte{'p'})
				} else {
					write([]byte{0, 0, 'p'})
				}
			} else if len(p) == 1 && p[0] <= 250 {
				if p[0] == 0 && !east {
					east = true
					s.east.Add(1)
				}
				if p[0] == 63 && !south {
					south = true
					s.south.Add(1)
				}
				select {
				case s.angles <- p[0]:
				default:
				}
			}
		}
	}))
	t.Cleanup(s.server.Close)
	return s
}
func waitFor(t *testing.T, fn func() bool) {
	t.Helper()
	end := time.Now().Add(6 * time.Second)
	for time.Now().Before(end) {
		if fn() {
			return
		}
		time.Sleep(10 * time.Millisecond)
	}
	t.Fatal("timed out")
}
func waitAngle(t *testing.T, s *testServer, want byte) {
	t.Helper()
	end := time.After(5 * time.Second)
	for {
		select {
		case angle := <-s.angles:
			if angle == want {
				return
			}
		case <-end:
			t.Fatalf("missing angle %d", want)
		}
	}
}
func TestOriginalEngineAndModernCompatibility(t *testing.T) {
	for _, modern := range []bool{false, true} {
		t.Run(map[bool]string{false: "original", true: "native"}[modern], func(t *testing.T) {
			s := newTestServer(t, modern)
			e, err := NewNativeEngine(&ProgramProperties{ServerAddress: strings.TrimPrefix(s.server.URL, "http://"), BotsPerProxy: 2})
			if err != nil {
				t.Fatal(err)
			}
			e.Target.Set(2000, 1000)
			ctx, cancel := context.WithCancel(context.Background())
			defer cancel()
			done := make(chan struct{})
			var active atomic.Int32
			go func() {
				defer close(done)
				e.Run(ctx, func(_ int, state string) {
					if state == "active" {
						active.Add(1)
					}
				})
			}()
			waitFor(t, func() bool { return active.Load() == 2 })
			waitAngle(t, s, 0)
			e.Target.Set(1000, 2000)
			waitAngle(t, s, 63)
			cancel()
			select {
			case <-done:
			case <-time.After(3 * time.Second):
				t.Fatal("worker did not stop")
			}
			waitFor(t, func() bool { return s.live.Load() == 0 })
		})
	}
}
func TestMalformedPackets(t *testing.T) {
	for _, packet := range [][]byte{{}, {0}, {0, 9, 'a'}, {0, 0}, {0, 1}} {
		if _, ok := nativeMessages(packet); ok {
			t.Errorf("accepted %v", packet)
		}
	}
	if _, err := NewNativeEngine(&ProgramProperties{BotsPerProxy: 0}); err == nil {
		t.Fatal("zero bots accepted")
	}
	if _, err := NewNativeEngine(&ProgramProperties{BotsPerProxy: 101}); err == nil {
		t.Fatal("unbounded bots accepted")
	}
	p := nativeProtocol{modern: true, version: 31, step: 42}
	w := &Earthworm{Identifier: 7, PositionX: 1000, PositionY: 1000}
	p.move(w, []byte{'G', 0, 0})
	if w.PositionX != 1042 || w.PositionY != 1000 {
		t.Fatal("angle movement")
	}
	p.move(w, []byte{'=', 64, 0, 3, 232, 7, 208})
	if w.PositionX != 1000 || w.PositionY != 2000 {
		t.Fatal("absolute movement")
	}
}
func TestNativeProcessBridge(t *testing.T) {
	bridge, worker := os.Getenv("SLITHER_MISSILE_BRIDGE_TEST"), os.Getenv("SLITHER_MISSILE_WORKER_TEST")
	if bridge == "" || worker == "" {
		t.Skip("native bridge binary not supplied")
	}
	root := filepath.Join(t.TempDir(), "native game \ud14c\uc2a4\ud2b8")
	dir := filepath.Join(root, "app/res/snake-missile")
	if err := os.MkdirAll(dir, 0700); err != nil {
		t.Fatal(err)
	}
	in, err := os.Open(worker)
	if err != nil {
		t.Fatal(err)
	}
	defer in.Close()
	out, err := os.Create(filepath.Join(dir, "worker.exe"))
	if err != nil {
		t.Fatal(err)
	}
	_, err = io.Copy(out, in)
	out.Close()
	if err != nil {
		t.Fatal(err)
	}
	s := newTestServer(t, true, 31)
	ctx, cancel := context.WithTimeout(context.Background(), 150*time.Second)
	defer cancel()
	cmd := exec.CommandContext(ctx, bridge, strings.TrimPrefix(s.server.URL, "http://"))
	cmd.Dir = root
	if output, err := cmd.CombinedOutput(); err != nil {
		t.Fatalf("native process bridge: %v\n%s", err, output)
	}
	waitFor(t, func() bool { return s.live.Load() == 0 })
	waitAngle(t, s, 0)
	waitAngle(t, s, 63)
	if s.east.Load() != 100 || s.south.Load() != 100 {
		t.Fatalf("not all 100 bots steered: east=%d south=%d", s.east.Load(), s.south.Load())
	}
}

func TestTotalCountAndHundredConnections(t *testing.T) {
	proxies := filepath.Join(t.TempDir(), "proxies.txt")
	if err := os.WriteFile(proxies, []byte("one:8080\ntwo:8080\n"), 0600); err != nil {
		t.Fatal(err)
	}
	e, err := NewNativeEngine(&ProgramProperties{TotalBots: 100, BotsPerProxy: 2, ProxyListPath: proxies})
	if err != nil || e.Count() != 100 {
		t.Fatal("UI count must be total, not multiplied by proxies", err)
	}
	s := newTestServer(t, true)
	e, err = NewNativeEngine(&ProgramProperties{TotalBots: 100, ServerAddress: strings.TrimPrefix(s.server.URL, "http://")})
	if err != nil {
		t.Fatal(err)
	}
	e.connectGap = time.Millisecond // scale coverage; native process test uses the real one-second pace
	e.Target.Set(2000, 1000)
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	done := make(chan struct{})
	var active atomic.Int32
	go func() {
		defer close(done)
		e.Run(ctx, func(_ int, state string) {
			if state == "active" {
				active.Add(1)
			}
		})
	}()
	deadline := time.Now().Add(15 * time.Second)
	for active.Load() < 100 && time.Now().Before(deadline) {
		time.Sleep(20 * time.Millisecond)
	}
	if active.Load() != 100 {
		t.Fatalf("only %d of 100 bots spawned", active.Load())
	}
	waitFor(t, func() bool { return s.east.Load() == 100 })
	e.Target.Set(1000, 2000)
	waitFor(t, func() bool { return s.south.Load() == 100 })
	cancel()
	select {
	case <-done:
	case <-time.After(3 * time.Second):
		t.Fatal("100 connections did not stop")
	}
	waitFor(t, func() bool { return s.live.Load() == 0 })
}

func TestServerRejectsSpawn(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		up := websocket.Upgrader{CheckOrigin: func(*http.Request) bool { return true }}
		c, err := up.Upgrade(w, r, nil)
		if err != nil {
			return
		}
		defer c.Close()
		_, _, _ = c.ReadMessage() // reject after HELLO, before any own-snake packet
	}))
	defer server.Close()
	e, err := NewNativeEngine(&ProgramProperties{TotalBots: 1, ServerAddress: strings.TrimPrefix(server.URL, "http://")})
	if err != nil {
		t.Fatal(err)
	}
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	states := make(chan string, 16)
	done := make(chan struct{})
	go func() { defer close(done); e.Run(ctx, func(_ int, s string) { states <- s }) }()
	timeout := time.After(5 * time.Second)
	for {
		select {
		case s := <-states:
			if s == "active" || s == "respawning" {
				t.Fatalf("rejection hidden by %s", s)
			}
			if s == "handshake_rejected" {
				cancel()
				<-done
				return
			}
		case <-timeout:
			t.Fatal("rejection not reported")
		}
	}
}
