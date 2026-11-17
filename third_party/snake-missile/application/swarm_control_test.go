package application

import (
	"math"
	"testing"
)

func TestSwarmSteering(t *testing.T) {
	c := SwarmControl{X: 1000, Y: 1000}
	if math.Abs(c.Heading(0, 1000, 0, 1)) > 1e-9 {
		t.Fatal("target east")
	}
	c.Mode = 2
	c.Angle = math.Pi / 2
	if math.Abs(c.Heading(9000, 3000, 0, 1)-math.Pi/2) > 1e-9 {
		t.Fatal("straight drift")
	}
	c.Mode = 3
	c.Angle = 0
	if c.Heading(0, 1500, 0, 1) >= 0 {
		t.Fatal("rain must return to lane")
	}
	if c.Heading(0, 500, 0, 1) <= 0 {
		t.Fatal("rain must return to lane")
	}
	c.Mode = 4
	if math.Abs(c.Heading(1600, 1000, 0, 1)-math.Pi/2) > 1e-9 {
		t.Fatal("orbit tangent")
	}
	if c.Heading(2000, 1000, 0, 1) <= math.Pi/2 {
		t.Fatal("orbit must turn inward")
	}
	for _, v := range []float64{math.NaN(), math.Inf(1), 1000001} {
		c.X = v
		if c.Valid() {
			t.Fatal("invalid coordinate")
		}
	}
	c = SwarmControl{Mode: 5}
	if c.Valid() {
		t.Fatal("unknown mode")
	}
}
