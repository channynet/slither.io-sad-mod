package application

import "math"

// SwarmControl is a local operator command. Coordinates are game-world units.
type SwarmControl struct {
	Mode        int
	X, Y, Angle float64
	Boost       bool
}

func (c SwarmControl) Valid() bool {
	return c.Mode >= 0 && c.Mode <= 4 && finite(c.X) && finite(c.Y) && finite(c.Angle) &&
		math.Abs(c.X) <= 1000000 && math.Abs(c.Y) <= 1000000 && math.Abs(c.Angle) <= 2*math.Pi
}
func finite(v float64) bool { return !math.IsNaN(v) && !math.IsInf(v, 0) }
func (t *NativeTarget) SetControl(c SwarmControl) bool {
	if !c.Valid() {
		return false
	}
	t.Lock()
	defer t.Unlock()
	t.X = int(c.X)
	t.Y = int(c.Y)
	t.control = c
	return true
}
func (t *NativeTarget) Control() SwarmControl {
	t.RLock()
	defer t.RUnlock()
	c := t.control
	c.X = float64(t.X)
	c.Y = float64(t.Y)
	return c
}

// Heading contains no network state. Mouse and follow differ in the client-supplied target.
func (c SwarmControl) Heading(x, y float64, index, total int) float64 {
	switch c.Mode {
	case 2:
		return c.Angle
	case 3:
		// Parallel rain lanes centered on the captured origin. Keep moving forwards.
		nx, ny := -math.Sin(c.Angle), math.Cos(c.Angle)
		lane := (float64(index) - float64(total-1)/2) * 80
		error := lane - ((x-c.X)*nx + (y-c.Y)*ny)
		correction := math.Max(-1000, math.Min(1000, error))
		return math.Atan2(800*math.Sin(c.Angle)+correction*ny, 800*math.Cos(c.Angle)+correction*nx)
	case 4:
		dx, dy := x-c.X, y-c.Y
		radius := 600 + float64(index%5)*90
		correction := math.Max(-1.2, math.Min(1.2, (math.Hypot(dx, dy)-radius)/radius))
		return math.Atan2(dy, dx) + math.Pi/2 + correction
	default:
		return math.Atan2(c.Y-y, c.X-x)
	}
}
