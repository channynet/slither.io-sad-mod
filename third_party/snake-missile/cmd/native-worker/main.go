package main

import (
	"bufio"
	"context"
	"encoding/json"
	"math"
	"net/url"
	"os"
	"strings"
	"sync"
	"sync/atomic"
	"time"

	"github.com/shift-jis/snake-missile/application"
)

type command struct {
	Command string `json:"command"`
	Server  string `json:"server"`
	Count   *int   `json:"count,omitempty"`
	X, Y    float64
	Mode    int     `json:"mode"`
	Angle   float64 `json:"angle"`
	Boost   bool    `json:"boost"`
}
type status struct {
	State      string `json:"state"`
	Active     int    `json:"active"`
	Total      int    `json:"total"`
	Connecting int    `json:"connecting"`
	Waiting    int    `json:"waiting"`
	Detail     string `json:"detail,omitempty"`
}

// Keep a pending failure visible while other worms connect successfully.
func summarize(states, details []string) status {
	s := status{State: "connecting", Total: len(states)}
	counts := map[string]int{}
	for _, state := range states {
		counts[state]++
		if state == "active" {
			s.Active++
		} else if state == "" || state == "connecting" || state == "connected" {
			s.Connecting++
		} else {
			s.Waiting++
		}
	}
	if s.Active == s.Total {
		s.State = "active"
		return s
	}
	for _, state := range []string{"handshake_rejected", "handshake_timeout", "connection_failed", "connection_closed"} {
		if counts[state] > 0 {
			s.State = state
			for i, value := range states {
				if value == state && i < len(details) && details[i] != "" {
					s.Detail = details[i]
					break
				}
			}
			return s
		}
	}
	if counts["respawning"] > 0 {
		s.State = "respawning"
	}
	return s
}

func main() {
	var output sync.Mutex
	write := func(s status) { output.Lock(); defer output.Unlock(); _ = json.NewEncoder(os.Stdout).Encode(s) }
	properties := application.ProgramProperties{BotsPerProxy: 1, TotalBots: 1, DirectOnly: true}
	ctx, cancel := context.WithCancel(context.Background())
	var engine *application.NativeEngine
	var lastCommand atomic.Int64
	lastCommand.Store(time.Now().UnixNano())
	go func() {
		tick := time.NewTicker(500 * time.Millisecond)
		defer tick.Stop()
		for {
			select {
			case <-ctx.Done():
				return
			case <-tick.C:
				if time.Since(time.Unix(0, lastCommand.Load())) > 5*time.Second {
					cancel()
					write(status{State: "control_timeout"})
					return
				}
			}
		}
	}()
	var workers sync.WaitGroup
	defer func() { cancel(); workers.Wait() }()
	scanner := bufio.NewScanner(os.Stdin)
	scanner.Buffer(make([]byte, 4096), 4096)
	lines := make(chan []byte, 1)
	go func() {
		defer close(lines)
		for scanner.Scan() {
			line := append([]byte(nil), scanner.Bytes()...)
			select {
			case lines <- line:
			case <-ctx.Done():
				return
			}
		}
	}()
	for {
		var line []byte
		select {
		case next, ok := <-lines:
			if !ok {
				return
			}
			line = next
		case <-ctx.Done():
			return
		}
		var message command
		if json.Unmarshal(line, &message) != nil {
			continue
		}
		lastCommand.Store(time.Now().UnixNano())
		if message.Command == "stop" {
			cancel()
			return
		}
		if message.Command == "control" && engine != nil {
			if !engine.Target.SetControl(application.SwarmControl{Mode: message.Mode, X: message.X, Y: message.Y, Angle: message.Angle, Boost: message.Boost}) {
				write(status{State: "invalid_target"})
			}
			continue
		}
		if message.Command == "target" && engine != nil {
			engine.Target.Set(message.X, message.Y)
			continue
		}
		if message.Command != "start" || engine != nil {
			continue
		}
		address := message.Server
		if !strings.HasPrefix(address, "ws://") && !strings.HasPrefix(address, "wss://") {
			address = "ws://" + address + "/slither"
		}
		u, err := url.Parse(address)
		if err != nil || u.Hostname() == "" || u.User != nil || (u.Scheme != "ws" && u.Scheme != "wss") || math.IsNaN(message.X) || math.IsNaN(message.Y) {
			write(status{State: "invalid_target"})
			continue
		}
		properties.ServerAddress = address
		if message.Count != nil {
			if *message.Count < 1 || *message.Count > 100 {
				write(status{State: "bot_count"})
				return
			}
			properties.TotalBots = *message.Count
		}
		engine, err = application.NewNativeEngine(&properties)
		if err != nil {
			write(status{State: err.Error()})
			return
		}
		if !engine.Target.SetControl(application.SwarmControl{Mode: message.Mode, X: message.X, Y: message.Y, Angle: message.Angle, Boost: message.Boost}) {
			write(status{State: "invalid_target"})
			return
		}
		states := make([]string, engine.Count())
		details := make([]string, engine.Count())
		var stateLock sync.Mutex
		engine.OnError = func(index int, detail string) {
			stateLock.Lock()
			defer stateLock.Unlock()
			details[index] = detail
		}
		workers.Add(1)
		go func() {
			defer workers.Done()
			tick := time.NewTicker(250 * time.Millisecond)
			defer tick.Stop()
			for {
				select {
				case <-ctx.Done():
					return
				case <-tick.C:
					stateLock.Lock()
					current := summarize(states, details)
					stateLock.Unlock()
					write(current)
				}
			}
		}()
		workers.Add(1)
		go func() {
			defer workers.Done()
			engine.Run(ctx, func(index int, state string) {
				stateLock.Lock()
				defer stateLock.Unlock()
				states[index] = state
				if state == "connecting" || state == "active" || state == "respawning" {
					details[index] = ""
				}
			})
		}()
	}
}
