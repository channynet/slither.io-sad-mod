package application

// Compatibility with SlitherNative's current raw/container protocol, adapted
// from app/src/network/callback.c (repository GPL license). Legacy packets still
// go through the upstream Snake Missile listeners unchanged.
import "math"

type nativeProtocol struct {
	modern      bool
	version     int
	step, angle float64
}

func nativeMessages(frame []byte) ([][]byte, bool) {
	if len(frame) == 0 {
		return nil, false
	}
	if frame[0] >= 32 {
		return [][]byte{frame}, true
	}
	var packets [][]byte
	for at := 0; at < len(frame); {
		n := 0
		if frame[at] < 32 {
			if at+1 >= len(frame) {
				return nil, false
			}
			n = int(frame[at])<<8 | int(frame[at+1])
			at += 2
		} else {
			n = int(frame[at]) - 32
			at++
		}
		if n < 1 || n > len(frame)-at {
			return nil, false
		}
		packets = append(packets, frame[at:at+n])
		at += n
	}
	return packets, true
}
func decodeNativeSecret(packet []byte) []byte {
	if len(packet) < 1 {
		return nil
	}
	decoded := make([]int, 92)
	shift := 23
	value := 0
	half := false
	index := 0
	for pos := 1; pos < len(packet) && pos < 184; pos++ {
		b := int(packet[pos])
		if b <= 96 {
			b += 32
		}
		b = (b - 97 - shift) % 26
		if b < 0 {
			b += 26
		}
		value = value*16 + b
		shift += 17
		if half {
			decoded[index] = int(byte(value))
			index++
			value = 0
		}
		half = !half
	}
	selected := append(append([]int{}, decoded[9:14]...), decoded[20:42]...)
	answer := make([]byte, 27)
	running := 0
	for i, a := range selected {
		offset := 65
		if a >= 97 {
			offset += 32
			a -= 32
		}
		a -= 65
		if i == 0 {
			running = 3 + a
		}
		answer[i] = byte((a+running)%26 + offset)
		running += 2 + a
	}
	return answer
}
func nativeSpawn(name string) []byte {
	packet := []byte{115, 30, 1, 35, 54, 206, 204, 169, 97, 178, 74, 136, 124, 117, 14, 210, 106, 236, 8, 208, 136, 213, 140, 111, 0, byte(len(name))}
	packet = append(packet, []byte(name)...)
	return append(packet, 0, 0)
}
func (p *nativeProtocol) receive(m *MissileManager, w *Earthworm, frame []byte) {
	if len(frame) == 0 {
		return
	}
	if frame[0] == '6' {
		p.modern = true
	}
	if !p.modern {
		if !validNativePacket(frame) {
			return
		}
		data := make([]int, len(frame))
		for i, b := range frame {
			data[i] = int(b)
		}
		if listener := m.ListenerFunctions[data[2]]; listener != nil {
			for _, packet := range listener(w, data) {
				w.SendPacket(packet)
			}
		}
		return
	}
	packets, ok := nativeMessages(frame)
	if !ok {
		return
	}
	for _, packet := range packets {
		switch packet[0] {
		case '6':
			answer := decodeNativeSecret(packet)
			if answer == nil {
				continue
			}
			w.SendPacket(answer)
			w.SendPacket(nativeSpawn(w.Nickname))
		case 'a':
			if len(packet) < 24 {
				continue
			}
			p.version = int(packet[23])
			p.step = 42
			if len(packet) > 24 && packet[24] > 0 {
				p.step = float64(packet[24])
			}
			w.IsInitialized = true
			w.NeedsPing = true
		case 'p':
			w.NeedsPing = true
		case 'v':
			w.IsDead = true
			w.IsConnected = false
		case 's':
			if len(packet) < 24 {
				continue
			}
			id := int(packet[1])<<8 | int(packet[2])
			if w.Identifier == 0 {
				w.Identifier = id
			}
			if id != w.Identifier {
				continue
			}
			w.PositionX = (int(packet[16])<<16 | int(packet[17])<<8 | int(packet[18])) / 5
			w.PositionY = (int(packet[19])<<16 | int(packet[20])<<8 | int(packet[21])) / 5
			p.angle = float64(int(packet[3])<<16|int(packet[4])<<8|int(packet[5])) * 2 * math.Pi / 16777215
		case 'g', 'n', 'G', 'N', '+', '=':
			p.move(w, packet)
		}
	}
}
func (p *nativeProtocol) move(w *Earthworm, packet []byte) {
	cmd := packet[0]
	size := len(packet) - 1
	at := 1
	angleMoves := p.version >= 15
	own := false
	if angleMoves {
		own = cmd == 'G' || cmd == 'N' || cmd == '=' && size == 6 || cmd == '+' && size == 9
	} else {
		own = cmd == 'g' && size == 4 || cmd == 'n' && size == 7 || cmd == 'G' && size == 2 || cmd == 'N' && size == 5
	}
	if !own {
		if len(packet) < 3 {
			return
		}
		if int(packet[1])<<8|int(packet[2]) != w.Identifier {
			return
		}
		at = 3
	}
	if w.Identifier == 0 {
		return
	}
	if !angleMoves {
		if cmd == 'g' || cmd == 'n' {
			if len(packet) < at+4 {
				return
			}
			w.PositionX = int(packet[at])<<8 | int(packet[at+1])
			w.PositionY = int(packet[at+2])<<8 | int(packet[at+3])
		} else if len(packet) >= at+2 {
			w.PositionX += int(packet[at]) - 128
			w.PositionY += int(packet[at+1]) - 128
		}
	} else if cmd == '+' || cmd == '=' {
		if len(packet) < at+6 {
			return
		}
		p.angle = float64(int(packet[at])<<8|int(packet[at+1])) * 2 * math.Pi / 65536
		w.PositionX = int(packet[at+2])<<8 | int(packet[at+3])
		w.PositionY = int(packet[at+4])<<8 | int(packet[at+5])
	} else {
		withAngle := cmd == 'G' && size == 2 || cmd == 'N' && size == 5 || cmd == 'g' && size == 4 || cmd == 'n' && size == 7
		if withAngle {
			if len(packet) < at+2 {
				return
			}
			p.angle = float64(int(packet[at])<<8|int(packet[at+1])) * 2 * math.Pi / 65536
		}
		w.PositionX += int(math.Cos(p.angle) * p.step)
		w.PositionY += int(math.Sin(p.angle) * p.step)
	}
}
