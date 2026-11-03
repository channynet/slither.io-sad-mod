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

	"github.com/shift-jis/snake-missile/application"
)

type command struct {
	Command string `json:"command"`
	Server  string `json:"server"`
	Count   *int   `json:"count,omitempty"`
	X, Y    float64
}
type status struct {
	State  string `json:"state"`
	Active int    `json:"active"`
	Total  int    `json:"total"`
}

func main() {
	var output sync.Mutex
	write := func(s status) { output.Lock(); defer output.Unlock(); _ = json.NewEncoder(os.Stdout).Encode(s) }
	properties := application.ProgramProperties{BotsPerProxy: 2}
	if data, err := os.ReadFile("snake-missile.json"); err == nil {
		if json.Unmarshal(data, &properties) != nil {
			write(status{State: "config_error"})
			return
		}
	} else if !os.IsNotExist(err) {
		write(status{State: "config_error"})
		return
	}
	ctx, cancel := context.WithCancel(context.Background())
	var engine *application.NativeEngine
	var workers sync.WaitGroup
	defer func() { cancel(); workers.Wait() }()
	scanner := bufio.NewScanner(os.Stdin)
	scanner.Buffer(make([]byte, 4096), 4096)
	for scanner.Scan() {
		var message command
		if json.Unmarshal(scanner.Bytes(), &message) != nil {
			continue
		}
		if message.Command == "stop" {
			cancel()
			return
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
		if !engine.Target.Set(message.X, message.Y) {
			write(status{State: "invalid_target"})
			return
		}
		states := make([]string, engine.Count())
		var stateLock sync.Mutex
		workers.Add(1)
		go func() {
			defer workers.Done()
			engine.Run(ctx, func(index int, state string) {
				stateLock.Lock()
				defer stateLock.Unlock()
				states[index] = state
				n := 0
				for _, s := range states {
					if s == "active" {
						n++
					}
				}
				write(status{State: state, Active: n, Total: len(states)})
			})
		}()
	}
	cancel() // EOF means the native app closed; never leave bots running.
}
