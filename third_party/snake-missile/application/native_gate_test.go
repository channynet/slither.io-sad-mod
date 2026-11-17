package application

import (
	"context"
	"sync"
	"testing"
	"time"
)

func TestSharedConnectionPacing(t *testing.T) {
	e, err := NewNativeEngine(&ProgramProperties{TotalBots: 4})
	if err != nil {
		t.Fatal(err)
	}
	if e.connectGap != time.Second || e.retryPause != 30*time.Second {
		t.Fatal("production connection limits changed")
	}
	e.connectGap = 20 * time.Millisecond
	ctx, cancel := context.WithTimeout(context.Background(), 3*time.Second)
	defer cancel()
	var workers sync.WaitGroup
	results := make(chan bool, 4)
	started := time.Now()
	for i := 0; i < 4; i++ {
		workers.Add(1)
		go func() { defer workers.Done(); results <- e.waitConnect(ctx) }()
	}
	workers.Wait()
	close(results)
	for allowed := range results {
		if !allowed {
			t.Fatal("connection gate timed out")
		}
	}
	if time.Since(started) < 3*e.connectGap {
		t.Fatal("parallel workers bypassed the shared connection gap")
	}
}

func TestConnectionCooldownAndCancellation(t *testing.T) {
	e := &NativeEngine{connectGap: time.Millisecond, retryPause: 200 * time.Millisecond}
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	if !e.waitConnect(ctx) {
		t.Fatal("first connection blocked")
	}
	e.coolDown()
	e.gate.Lock()
	until := e.nextConnect
	e.gate.Unlock()
	result := make(chan bool, 1)
	go func() { result <- e.waitConnect(ctx) }()
	select {
	case allowed := <-result:
		if !allowed || time.Now().Before(until) {
			t.Fatal("cooldown bypassed")
		}
	case <-time.After(3 * time.Second):
		t.Fatal("cooldown did not end")
	}
	e.coolDown()
	go func() { result <- e.waitConnect(ctx) }()
	cancel()
	select {
	case allowed := <-result:
		if allowed {
			t.Fatal("canceled connection allowed")
		}
	case <-time.After(time.Second):
		t.Fatal("stop waited for the cooldown")
	}
}
