package consistenthash

import (
	"testing"
)

func TestRingHasher(t *testing.T) {
	h := NewRingHasher()
	defer h.Close()

	// Add nodes
	h.AddNode(Node{ID: "node1", Address: "10.0.0.1:11211", Weight: 100})
	h.AddNode(Node{ID: "node2", Address: "10.0.0.2:11211", Weight: 100})
	h.AddNode(Node{ID: "node3", Address: "10.0.0.3:11211", Weight: 100})

	// Test lookup
	node, ok := h.Lookup("test-key")
	if !ok {
		t.Fatal("expected to find a node")
	}
	if node != "node1" && node != "node2" && node != "node3" {
		t.Errorf("unexpected node: %s", node)
	}

	// Test remove
	h.RemoveNode("node1")
	node, ok = h.Lookup("test-key")
	if !ok {
		t.Fatal("expected to find a node after removal")
	}
	if node == "node1" {
		t.Error("removed node should not be returned")
	}
}

func TestJumpHasher(t *testing.T) {
	h := NewJumpHasher()
	defer h.Close()

	h.AddNode(Node{ID: "node1", Address: "10.0.0.1:11211", Weight: 100})
	h.AddNode(Node{ID: "node2", Address: "10.0.0.2:11211", Weight: 100})

	node, ok := h.Lookup("test-key")
	if !ok {
		t.Fatal("expected to find a node")
	}
	if node != "node1" && node != "node2" {
		t.Errorf("unexpected node: %s", node)
	}
}

func TestMaglevHasher(t *testing.T) {
	h := NewMaglevHasher(65537)
	defer h.Close()

	h.AddNode(Node{ID: "node1", Address: "10.0.0.1:11211", Weight: 100})
	h.AddNode(Node{ID: "node2", Address: "10.0.0.2:11211", Weight: 100})
	h.AddNode(Node{ID: "node3", Address: "10.0.0.3:11211", Weight: 100})

	node, ok := h.Lookup("test-key")
	if !ok {
		t.Fatal("expected to find a node")
	}
	if node != "node1" && node != "node2" && node != "node3" {
		t.Errorf("unexpected node: %s", node)
	}
}

func TestRendezvousHasher(t *testing.T) {
	h := NewRendezvousHasher()
	defer h.Close()

	h.AddNode(Node{ID: "node1", Address: "10.0.0.1:11211", Weight: 100})
	h.AddNode(Node{ID: "node2", Address: "10.0.0.2:11211", Weight: 100})

	node, ok := h.Lookup("test-key")
	if !ok {
		t.Fatal("expected to find a node")
	}
	if node != "node1" && node != "node2" {
		t.Errorf("unexpected node: %s", node)
	}
}

func TestMultiProbeHasher(t *testing.T) {
	h := NewMultiProbeHasher(16)
	defer h.Close()

	h.AddNode(Node{ID: "node1", Address: "10.0.0.1:11211", Weight: 100})
	h.AddNode(Node{ID: "node2", Address: "10.0.0.2:11211", Weight: 100})

	node, ok := h.Lookup("test-key")
	if !ok {
		t.Fatal("expected to find a node")
	}
	if node != "node1" && node != "node2" {
		t.Errorf("unexpected node: %s", node)
	}
}

func TestBoundedLoadsHasher(t *testing.T) {
	h := NewBoundedLoadsHasher(150, 1000)
	defer h.Close()

	h.AddNode(Node{ID: "node1", Address: "10.0.0.1:11211", Weight: 100})
	h.AddNode(Node{ID: "node2", Address: "10.0.0.2:11211", Weight: 100})

	node, ok := h.Lookup("test-key")
	if !ok {
		t.Fatal("expected to find a node")
	}
	if node != "node1" && node != "node2" {
		t.Errorf("unexpected node: %s", node)
	}
}

func TestKetamaHasher(t *testing.T) {
	h := NewKetamaHasher()
	defer h.Close()

	h.AddNode(Node{ID: "node1", Address: "10.0.0.1:11211", Weight: 100})
	h.AddNode(Node{ID: "node2", Address: "10.0.0.2:11211", Weight: 100})

	node, ok := h.Lookup("test-key")
	if !ok {
		t.Fatal("expected to find a node")
	}
	if node != "node1" && node != "node2" {
		t.Errorf("unexpected node: %s", node)
	}

	// Test weighted distribution
	h2 := NewKetamaHasher()
	defer h2.Close()

	h2.AddNode(Node{ID: "heavy", Address: "10.0.0.1:11211", Weight: 300})
	h2.AddNode(Node{ID: "light", Address: "10.0.0.2:11211", Weight: 100})

	counts := make(map[string]int)
	for i := 0; i < 10000; i++ {
		node, ok := h2.Lookup("key-" + string(rune(i)))
		if ok {
			counts[node]++
		}
	}

	ratio := float64(counts["heavy"]) / float64(counts["light"])
	if ratio < 2.0 || ratio > 4.0 {
		t.Errorf("expected ratio ~3.0, got %.2f", ratio)
	}
}

func TestAnchorHasher(t *testing.T) {
	h := NewAnchorHasher()
	defer h.Close()

	h.AddNode(Node{ID: "node1", Address: "10.0.0.1:11211", Weight: 100})
	h.AddNode(Node{ID: "node2", Address: "10.0.0.2:11211", Weight: 100})

	node, ok := h.Lookup("test-key")
	if !ok {
		t.Fatal("expected to find a node")
	}
	if node != "node1" && node != "node2" {
		t.Errorf("unexpected node: %s", node)
	}
}

func TestWeightedDistribution(t *testing.T) {
	// Test that all hashers respect weights
	hashers := []struct {
		name string
		h    Hasher
	}{
		{"Ring", NewRingHasher()},
		{"Jump", NewJumpHasher()},
		{"Maglev", NewMaglevHasher(65537)},
		{"Rendezvous", NewRendezvousHasher()},
		{"MultiProbe", NewMultiProbeHasher(16)},
		{"BoundedLoads", NewBoundedLoadsHasher(150, 1000)},
		{"Ketama", NewKetamaHasher()},
		{"Anchor", NewAnchorHasher()},
	}

	for _, tc := range hashers {
		defer tc.h.Close()

		tc.h.AddNode(Node{ID: "heavy", Weight: 300})
		tc.h.AddNode(Node{ID: "light", Weight: 100})

		counts := make(map[string]int)
		for i := 0; i < 10000; i++ {
			node, ok := tc.h.Lookup("key-" + string(rune(i)))
			if ok {
				counts[node]++
			}
		}

		ratio := float64(counts["heavy"]) / float64(counts["light"])
		if ratio < 2.0 || ratio > 4.0 {
			t.Errorf("%s: expected ratio ~3.0, got %.2f", tc.name, ratio)
		}
	}
}