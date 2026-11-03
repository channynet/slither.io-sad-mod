package application

// Native adaptation of the original manager: one state owner and one WebSocket
// reader per worm. The original listeners and steering are reused, with a live
// destination instead of (32000, 32000). Cancellation closes all connections.
import (
	"context"
	"errors"
	"math"
	"sync"
	"time"

	"github.com/fasthttp/websocket"
	"go.uber.org/zap"
)

type NativeTarget struct {
	sync.RWMutex
	X, Y int
}

func (t *NativeTarget) Set(x, y float64) bool {
	if math.IsNaN(x) || math.IsNaN(y) || math.IsInf(x, 0) || math.IsInf(y, 0) || math.Abs(x) > 1000000 || math.Abs(y) > 1000000 {
		return false
	}
	t.Lock()
	t.X = int(x)
	t.Y = int(y)
	t.Unlock()
	return true
}
func (t *NativeTarget) Point() (int, int) { t.RLock(); defer t.RUnlock(); return t.X, t.Y }

type NativeEngine struct {
	manager *MissileManager
	proxies []string
	total   int
	Target  NativeTarget
}

func NewNativeEngine(properties *ProgramProperties) (*NativeEngine, error) {
	proxies, err := properties.ReadProxyList()
	if err != nil {
		return nil, errors.New("proxy_file")
	}
	total := properties.TotalBots
	if total == 0 && properties.BotsPerProxy >= 1 && properties.BotsPerProxy <= 100 && len(proxies) <= 100 {
		total = len(proxies) * properties.BotsPerProxy
	}
	if len(proxies) < 1 || total < 1 || total > 100 {
		return nil, errors.New("bot_count")
	}
	m := &MissileManager{Properties: properties, Logger: zap.NewNop(), ListenerFunctions: make(map[int]ListenerFunc)}
	m.InitializeListeners()
	return &NativeEngine{manager: m, proxies: proxies, total: total}, nil
}
func (e *NativeEngine) Count() int { return e.total }
func (e *NativeEngine) Run(ctx context.Context, report func(int, string)) {
	var workers sync.WaitGroup
	for index := 0; index < e.Count(); index++ {
		if index > 0 {
			select {
			case <-ctx.Done():
				workers.Wait()
				return
			case <-time.After(50 * time.Millisecond):
			}
		}
		workers.Add(1)
		go func(index int) { defer workers.Done(); e.runWorm(ctx, index, report) }(index)
	}
	workers.Wait()
}
func (e *NativeEngine) runWorm(ctx context.Context, index int, report func(int, string)) {
	delay := time.Second
	for ctx.Err() == nil {
		worm := NewEarthworm(e.manager, e.proxies[index%len(e.proxies)])
		report(index, "connecting")
		if err := worm.ConnectToServerContext(ctx, e.manager.Properties.ServerAddress); err != nil {
			report(index, "connection_failed")
		} else {
			report(index, "connected")
			state := e.serve(ctx, worm, index, report)
			_ = worm.Connection.Close()
			report(index, state)
		}
		select {
		case <-ctx.Done():
			return
		case <-time.After(delay):
		}
		if delay < 15*time.Second {
			delay *= 2
		}
	}
}

type receivedPacket struct {
	kind int
	data []byte
	err  error
}

func (e *NativeEngine) serve(ctx context.Context, worm *Earthworm, index int, report func(int, string)) string {
	ctx, cancel := context.WithCancel(ctx)
	defer cancel()
	incoming := make(chan receivedPacket, 8)
	readerDone := make(chan struct{})
	worm.Connection.SetReadLimit(2 * 1024 * 1024)
	go func() {
		defer close(readerDone)
		for {
			kind, data, err := worm.Connection.ReadMessage()
			select {
			case incoming <- receivedPacket{kind, data, err}:
			case <-ctx.Done():
				return
			}
			if err != nil {
				return
			}
		}
	}()
	defer func() { cancel(); _ = worm.Connection.Close(); <-readerDone }()
	worm.SendPacket([]byte{99}) // original HELLO
	tick := time.NewTicker(100 * time.Millisecond)
	defer tick.Stop()
	protocol := nativeProtocol{}
	initialized := false
	connectedAt := time.Now()
	for worm.IsConnected && !worm.IsDead {
		select {
		case <-ctx.Done():
			return "stopped"
		case packet := <-incoming:
			if packet.err != nil {
				if !initialized {
					return "handshake_rejected"
				}
				return "connection_closed"
			}
			if packet.kind != websocket.BinaryMessage {
				continue
			}
			protocol.receive(e.manager, worm, packet.data)
			if worm.IsInitialized && worm.Identifier != 0 && !initialized {
				initialized = true
				report(index, "active")
			}
		case <-tick.C:
			if !initialized && time.Since(connectedAt) > 20*time.Second {
				return "handshake_timeout"
			}
			if initialized {
				x, y := e.Target.Point()
				worm.UpdateAngleTowardsPoint(x, y)
				if protocol.modern {
					worm.CurrentAngle = math.Min(250, math.Round(math.Mod(math.Atan2(float64(y-worm.PositionY), float64(x-worm.PositionX))*251/(2*math.Pi)+251, 251)))
				}
				worm.UpdateState()
			}
		}
	}
	return "respawning"
}
func validNativePacket(data []byte) bool {
	if len(data) < 3 {
		return false
	}
	minimum := 3
	switch data[2] {
	case '6':
		minimum = 65
	case 's':
		minimum = 31
	case 'n', 'g':
		minimum = 9
	case 'G', 'N':
		minimum = 7
	}
	return len(data) >= minimum
}
