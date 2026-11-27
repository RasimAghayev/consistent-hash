// Consistent Hash Go Bindings
// This provides a Go wrapper around the C++ consistent_hash library using CGO

package consistenthash

/*
#cgo CXXFLAGS: -std=c++20 -I${SRCDIR}/../../../include
#cgo LDFLAGS: -L${SRCDIR}/../../../build -lconsistent_hash -lstdc++

#include <stdlib.h>
#include <string.h>
#include <consistent_hash/ring.hpp>
#include <consistent_hash/jump.hpp>
#include <consistent_hash/maglev.hpp>
#include <consistent_hash/rendezvous.hpp>
#include <consistent_hash/multiprobe.hpp>
#include <consistent_hash/bounded_loads.hpp>
#include <consistent_hash/ketama_anchor.hpp>

using namespace consistent_hash;

// Ring Hasher
typedef RingHasher<> RingHasher;

RingHasher* ring_hasher_create() {
    return new RingHasher();
}

void ring_hasher_destroy(RingHasher* h) {
    delete h;
}

void ring_hasher_add_node(RingHasher* h, const char* id, const char* address, uint32_t weight, const char* zone) {
    Node<> node;
    node.id = id;
    node.address = address;
    node.weight = weight;
    node.zone = zone;
    node.status = NodeStatus::Active;
    h->add_node(node);
}

void ring_hasher_remove_node(RingHasher* h, const char* id) {
    h->remove_node(id);
}

const char* ring_hasher_lookup(RingHasher* h, const char* key) {
    auto opt = h->lookup(key);
    if (opt) {
        char* result = (char*)malloc(opt->size() + 1);
        strcpy(result, opt->c_str());
        return result;
    }
    return NULL;
}

void ring_hasher_free_string(const char* s) {
    free((void*)s);
}

// Jump Hasher
typedef JumpHasher<> JumpHasher;

JumpHasher* jump_hasher_create() {
    return new JumpHasher();
}

void jump_hasher_destroy(JumpHasher* h) {
    delete h;
}

void jump_hasher_add_node(JumpHasher* h, const char* id, const char* address, uint32_t weight, const char* zone) {
    Node<> node;
    node.id = id;
    node.address = address;
    node.weight = weight;
    node.zone = zone;
    node.status = NodeStatus::Active;
    h->add_node(node);
}

void jump_hasher_remove_node(JumpHasher* h, const char* id) {
    h->remove_node(id);
}

const char* jump_hasher_lookup(JumpHasher* h, const char* key) {
    auto opt = h->lookup(key);
    if (opt) {
        char* result = (char*)malloc(opt->size() + 1);
        strcpy(result, opt->c_str());
        return result;
    }
    return NULL;
}

// Maglev Hasher
typedef MaglevHasher<> MaglevHasher;

MaglevHasher* maglev_hasher_create(size_t table_size) {
    return new MaglevHasher(table_size);
}

void maglev_hasher_destroy(MaglevHasher* h) {
    delete h;
}

void maglev_hasher_add_node(MaglevHasher* h, const char* id, const char* address, uint32_t weight, const char* zone) {
    Node<> node;
    node.id = id;
    node.address = address;
    node.weight = weight;
    node.zone = zone;
    node.status = NodeStatus::Active;
    h->add_node(node);
}

void maglev_hasher_remove_node(MaglevHasher* h, const char* id) {
    h->remove_node(id);
}

const char* maglev_hasher_lookup(MaglevHasher* h, const char* key) {
    auto opt = h->lookup(key);
    if (opt) {
        char* result = (char*)malloc(opt->size() + 1);
        strcpy(result, opt->c_str());
        return result;
    }
    return NULL;
}

// Rendezvous Hasher
typedef RendezvousHasher<> RendezvousHasher;

RendezvousHasher* rendezvous_hasher_create() {
    return new RendezvousHasher();
}

void rendezvous_hasher_destroy(RendezvousHasher* h) {
    delete h;
}

void rendezvous_hasher_add_node(RendezvousHasher* h, const char* id, const char* address, uint32_t weight, const char* zone) {
    Node<> node;
    node.id = id;
    node.address = address;
    node.weight = weight;
    node.zone = zone;
    node.status = NodeStatus::Active;
    h->add_node(node);
}

void rendezvous_hasher_remove_node(RendezvousHasher* h, const char* id) {
    h->remove_node(id);
}

const char* rendezvous_hasher_lookup(RendezvousHasher* h, const char* key) {
    auto opt = h->lookup(key);
    if (opt) {
        char* result = (char*)malloc(opt->size() + 1);
        strcpy(result, opt->c_str());
        return result;
    }
    return NULL;
}

// Multi-Probe Hasher
typedef MultiProbeHasher<> MultiProbeHasher;

MultiProbeHasher* multiprobe_hasher_create(size_t probes) {
    return new MultiProbeHasher(probes);
}

void multiprobe_hasher_destroy(MultiProbeHasher* h) {
    delete h;
}

void multiprobe_hasher_add_node(MultiProbeHasher* h, const char* id, const char* address, uint32_t weight, const char* zone) {
    Node<> node;
    node.id = id;
    node.address = address;
    node.weight = weight;
    node.zone = zone;
    node.status = NodeStatus::Active;
    h->add_node(node);
}

void multiprobe_hasher_remove_node(MultiProbeHasher* h, const char* id) {
    h->remove_node(id);
}

const char* multiprobe_hasher_lookup(MultiProbeHasher* h, const char* key) {
    auto opt = h->lookup(key);
    if (opt) {
        char* result = (char*)malloc(opt->size() + 1);
        strcpy(result, opt->c_str());
        return result;
    }
    return NULL;
}

// Bounded Loads Hasher
typedef BoundedLoadsHasher<> BoundedLoadsHasher;

BoundedLoadsHasher* bounded_loads_hasher_create(size_t vnodes, size_t capacity) {
    return new BoundedLoadsHasher(vnodes, capacity);
}

void bounded_loads_hasher_destroy(BoundedLoadsHasher* h) {
    delete h;
}

void bounded_loads_hasher_add_node(BoundedLoadsHasher* h, const char* id, const char* address, uint32_t weight, const char* zone) {
    Node<> node;
    node.id = id;
    node.address = address;
    node.weight = weight;
    node.zone = zone;
    node.status = NodeStatus::Active;
    h->add_node(node);
}

void bounded_loads_hasher_remove_node(BoundedLoadsHasher* h, const char* id) {
    h->remove_node(id);
}

const char* bounded_loads_hasher_lookup(BoundedLoadsHasher* h, const char* key) {
    auto opt = h->lookup(key);
    if (opt) {
        char* result = (char*)malloc(opt->size() + 1);
        strcpy(result, opt->c_str());
        return result;
    }
    return NULL;
}

// Ketama Hasher
typedef KetamaHasher<> KetamaHasher;

KetamaHasher* ketama_hasher_create() {
    return new KetamaHasher();
}

void ketama_hasher_destroy(KetamaHasher* h) {
    delete h;
}

void ketama_hasher_add_node(KetamaHasher* h, const char* id, const char* address, uint32_t weight, const char* zone) {
    Node<> node;
    node.id = id;
    node.address = address;
    node.weight = weight;
    node.zone = zone;
    node.status = NodeStatus::Active;
    h->add_node(node);
}

void ketama_hasher_remove_node(KetamaHasher* h, const char* id) {
    h->remove_node(id);
}

const char* ketama_hasher_lookup(KetamaHasher* h, const char* key) {
    auto opt = h->lookup(key);
    if (opt) {
        char* result = (char*)malloc(opt->size() + 1);
        strcpy(result, opt->c_str());
        return result;
    }
    return NULL;
}

// Anchor Hasher
typedef AnchorHasher<> AnchorHasher;

AnchorHasher* anchor_hasher_create() {
    return new AnchorHasher();
}

void anchor_hasher_destroy(AnchorHasher* h) {
    delete h;
}

void anchor_hasher_add_node(AnchorHasher* h, const char* id, const char* address, uint32_t weight, const char* zone) {
    Node<> node;
    node.id = id;
    node.address = address;
    node.weight = weight;
    node.zone = zone;
    node.status = NodeStatus::Active;
    h->add_node(node);
}

void anchor_hasher_remove_node(AnchorHasher* h, const char* id) {
    h->remove_node(id);
}

const char* anchor_hasher_lookup(AnchorHasher* h, const char* key) {
    auto opt = h->lookup(key);
    if (opt) {
        char* result = (char*)malloc(opt->size() + 1);
        strcpy(result, opt->c_str());
        return result;
    }
    return NULL;
}
*/
import "C"
import (
	"runtime"
	"unsafe"
)

// Node represents a node in the consistent hash ring
type Node struct {
	ID      string
	Address string
	Weight  uint32
	Zone    string
}

// Hasher is the common interface for all consistent hash algorithms
type Hasher interface {
	AddNode(node Node)
	RemoveNode(id string)
	Lookup(key string) (string, bool)
	Close()
}

// RingHasher implements ring-based consistent hashing with virtual nodes
type RingHasher struct {
	ptr *C.RingHasher
}

// NewRingHasher creates a new ring-based consistent hasher
func NewRingHasher() *RingHasher {
	h := &RingHasher{
		ptr: C.ring_hasher_create(),
	}
	runtime.SetFinalizer(h, func(h *RingHasher) {
		C.ring_hasher_destroy(h.ptr)
	})
	return h
}

// AddNode adds a node to the ring
func (h *RingHasher) AddNode(node Node) {
	cID := C.CString(node.ID)
	cAddress := C.CString(node.Address)
	cZone := C.CString(node.Zone)
	defer C.free(unsafe.Pointer(cID))
	defer C.free(unsafe.Pointer(cAddress))
	defer C.free(unsafe.Pointer(cZone))

	C.ring_hasher_add_node(h.ptr, cID, cAddress, C.uint32_t(node.Weight), cZone)
}

// RemoveNode removes a node from the ring
func (h *RingHasher) RemoveNode(id string) {
	cID := C.CString(id)
	defer C.free(unsafe.Pointer(cID))
	C.ring_hasher_remove_node(h.ptr, cID)
}

// Lookup finds the node responsible for a key
func (h *RingHasher) Lookup(key string) (string, bool) {
	cKey := C.CString(key)
	defer C.free(unsafe.Pointer(cKey))

	cResult := C.ring_hasher_lookup(h.ptr, cKey)
	if cResult == nil {
		return "", false
	}
	defer C.ring_hasher_free_string(cResult)
	return C.GoString(cResult), true
}

// Close destroys the hasher
func (h *RingHasher) Close() {
	if h.ptr != nil {
		C.ring_hasher_destroy(h.ptr)
		h.ptr = nil
	}
}

// JumpHasher implements Google's jump consistent hashing
type JumpHasher struct {
	ptr *C.JumpHasher
}

// NewJumpHasher creates a new jump consistent hasher
func NewJumpHasher() *JumpHasher {
	h := &JumpHasher{
		ptr: C.jump_hasher_create(),
	}
	runtime.SetFinalizer(h, func(h *JumpHasher) {
		C.jump_hasher_destroy(h.ptr)
	})
	return h
}

func (h *JumpHasher) AddNode(node Node) {
	cID := C.CString(node.ID)
	cAddress := C.CString(node.Address)
	cZone := C.CString(node.Zone)
	defer C.free(unsafe.Pointer(cID))
	defer C.free(unsafe.Pointer(cAddress))
	defer C.free(unsafe.Pointer(cZone))

	C.jump_hasher_add_node(h.ptr, cID, cAddress, C.uint32_t(node.Weight), cZone)
}

func (h *JumpHasher) RemoveNode(id string) {
	cID := C.CString(id)
	defer C.free(unsafe.Pointer(cID))
	C.jump_hasher_remove_node(h.ptr, cID)
}

func (h *JumpHasher) Lookup(key string) (string, bool) {
	cKey := C.CString(key)
	defer C.free(unsafe.Pointer(cKey))

	cResult := C.jump_hasher_lookup(h.ptr, cKey)
	if cResult == nil {
		return "", false
	}
	defer C.ring_hasher_free_string(cResult)
	return C.GoString(cResult), true
}

func (h *JumpHasher) Close() {
	if h.ptr != nil {
		C.jump_hasher_destroy(h.ptr)
		h.ptr = nil
	}
}

// MaglevHasher implements Google's Maglev consistent hashing
type MaglevHasher struct {
	ptr *C.MaglevHasher
}

// NewMaglevHasher creates a new Maglev hasher with the given table size
func NewMaglevHasher(tableSize int) *MaglevHasher {
	h := &MaglevHasher{
		ptr: C.maglev_hasher_create(C.size_t(tableSize)),
	}
	runtime.SetFinalizer(h, func(h *MaglevHasher) {
		C.maglev_hasher_destroy(h.ptr)
	})
	return h
}

func (h *MaglevHasher) AddNode(node Node) {
	cID := C.CString(node.ID)
	cAddress := C.CString(node.Address)
	cZone := C.CString(node.Zone)
	defer C.free(unsafe.Pointer(cID))
	defer C.free(unsafe.Pointer(cAddress))
	defer C.free(unsafe.Pointer(cZone))

	C.maglev_hasher_add_node(h.ptr, cID, cAddress, C.uint32_t(node.Weight), cZone)
}

func (h *MaglevHasher) RemoveNode(id string) {
	cID := C.CString(id)
	defer C.free(unsafe.Pointer(cID))
	C.maglev_hasher_remove_node(h.ptr, cID)
}

func (h *MaglevHasher) Lookup(key string) (string, bool) {
	cKey := C.CString(key)
	defer C.free(unsafe.Pointer(cKey))

	cResult := C.maglev_hasher_lookup(h.ptr, cKey)
	if cResult == nil {
		return "", false
	}
	defer C.ring_hasher_free_string(cResult)
	return C.GoString(cResult), true
}

func (h *MaglevHasher) Close() {
	if h.ptr != nil {
		C.maglev_hasher_destroy(h.ptr)
		h.ptr = nil
	}
}

// RendezvousHasher implements Rendezvous (HRW) consistent hashing
type RendezvousHasher struct {
	ptr *C.RendezvousHasher
}

// NewRendezvousHasher creates a new Rendezvous hasher
func NewRendezvousHasher() *RendezvousHasher {
	h := &RendezvousHasher{
		ptr: C.rendezvous_hasher_create(),
	}
	runtime.SetFinalizer(h, func(h *RendezvousHasher) {
		C.rendezvous_hasher_destroy(h.ptr)
	})
	return h
}

func (h *RendezvousHasher) AddNode(node Node) {
	cID := C.CString(node.ID)
	cAddress := C.CString(node.Address)
	cZone := C.CString(node.Zone)
	defer C.free(unsafe.Pointer(cID))
	defer C.free(unsafe.Pointer(cAddress))
	defer C.free(unsafe.Pointer(cZone))

	C.rendezvous_hasher_add_node(h.ptr, cID, cAddress, C.uint32_t(node.Weight), cZone)
}

func (h *RendezvousHasher) RemoveNode(id string) {
	cID := C.CString(id)
	defer C.free(unsafe.Pointer(cID))
	C.rendezvous_hasher_remove_node(h.ptr, cID)
}

func (h *RendezvousHasher) Lookup(key string) (string, bool) {
	cKey := C.CString(key)
	defer C.free(unsafe.Pointer(cKey))

	cResult := C.rendezvous_hasher_lookup(h.ptr, cKey)
	if cResult == nil {
		return "", false
	}
	defer C.ring_hasher_free_string(cResult)
	return C.GoString(cResult), true
}

func (h *RendezvousHasher) Close() {
	if h.ptr != nil {
		C.rendezvous_hasher_destroy(h.ptr)
		h.ptr = nil
	}
}

// MultiProbeHasher implements multi-probe consistent hashing
type MultiProbeHasher struct {
	ptr *C.MultiProbeHasher
}

// NewMultiProbeHasher creates a new multi-probe hasher
func NewMultiProbeHasher(probes int) *MultiProbeHasher {
	h := &MultiProbeHasher{
		ptr: C.multiprobe_hasher_create(C.size_t(probes)),
	}
	runtime.SetFinalizer(h, func(h *MultiProbeHasher) {
		C.multiprobe_hasher_destroy(h.ptr)
	})
	return h
}

func (h *MultiProbeHasher) AddNode(node Node) {
	cID := C.CString(node.ID)
	cAddress := C.CString(node.Address)
	cZone := C.CString(node.Zone)
	defer C.free(unsafe.Pointer(cID))
	defer C.free(unsafe.Pointer(cAddress))
	defer C.free(unsafe.Pointer(cZone))

	C.multiprobe_hasher_add_node(h.ptr, cID, cAddress, C.uint32_t(node.Weight), cZone)
}

func (h *MultiProbeHasher) RemoveNode(id string) {
	cID := C.CString(id)
	defer C.free(unsafe.Pointer(cID))
	C.multiprobe_hasher_remove_node(h.ptr, cID)
}

func (h *MultiProbeHasher) Lookup(key string) (string, bool) {
	cKey := C.CString(key)
	defer C.free(unsafe.Pointer(cKey))

	cResult := C.multiprobe_hasher_lookup(h.ptr, cKey)
	if cResult == nil {
		return "", false
	}
	defer C.ring_hasher_free_string(cResult)
	return C.GoString(cResult), true
}

func (h *MultiProbeHasher) Close() {
	if h.ptr != nil {
		C.multiprobe_hasher_destroy(h.ptr)
		h.ptr = nil
	}
}

// BoundedLoadsHasher implements bounded-loads consistent hashing
type BoundedLoadsHasher struct {
	ptr *C.BoundedLoadsHasher
}

// NewBoundedLoadsHasher creates a new bounded-loads hasher
func NewBoundedLoadsHasher(vnodes, capacity int) *BoundedLoadsHasher {
	h := &BoundedLoadsHasher{
		ptr: C.bounded_loads_hasher_create(C.size_t(vnodes), C.size_t(capacity)),
	}
	runtime.SetFinalizer(h, func(h *BoundedLoadsHasher) {
		C.bounded_loads_hasher_destroy(h.ptr)
	})
	return h
}

func (h *BoundedLoadsHasher) AddNode(node Node) {
	cID := C.CString(node.ID)
	cAddress := C.CString(node.Address)
	cZone := C.CString(node.Zone)
	defer C.free(unsafe.Pointer(cID))
	defer C.free(unsafe.Pointer(cAddress))
	defer C.free(unsafe.Pointer(cZone))

	C.bounded_loads_hasher_add_node(h.ptr, cID, cAddress, C.uint32_t(node.Weight), cZone)
}

func (h *BoundedLoadsHasher) RemoveNode(id string) {
	cID := C.CString(id)
	defer C.free(unsafe.Pointer(cID))
	C.bounded_loads_hasher_remove_node(h.ptr, cID)
}

func (h *BoundedLoadsHasher) Lookup(key string) (string, bool) {
	cKey := C.CString(key)
	defer C.free(unsafe.Pointer(cKey))

	cResult := C.bounded_loads_hasher_lookup(h.ptr, cKey)
	if cResult == nil {
		return "", false
	}
	defer C.ring_hasher_free_string(cResult)
	return C.GoString(cResult), true
}

func (h *BoundedLoadsHasher) Close() {
	if h.ptr != nil {
		C.bounded_loads_hasher_destroy(h.ptr)
		h.ptr = nil
	}
}

// KetamaHasher implements Memcached-compatible Ketama consistent hashing
type KetamaHasher struct {
	ptr *C.KetamaHasher
}

// NewKetamaHasher creates a new Ketama hasher
func NewKetamaHasher() *KetamaHasher {
	h := &KetamaHasher{
		ptr: C.ketama_hasher_create(),
	}
	runtime.SetFinalizer(h, func(h *KetamaHasher) {
		C.ketama_hasher_destroy(h.ptr)
	})
	return h
}

func (h *KetamaHasher) AddNode(node Node) {
	cID := C.CString(node.ID)
	cAddress := C.CString(node.Address)
	cZone := C.CString(node.Zone)
	defer C.free(unsafe.Pointer(cID))
	defer C.free(unsafe.Pointer(cAddress))
	defer C.free(unsafe.Pointer(cZone))

	C.ketama_hasher_add_node(h.ptr, cID, cAddress, C.uint32_t(node.Weight), cZone)
}

func (h *KetamaHasher) RemoveNode(id string) {
	cID := C.CString(id)
	defer C.free(unsafe.Pointer(cID))
	C.ketama_hasher_remove_node(h.ptr, cID)
}

func (h *KetamaHasher) Lookup(key string) (string, bool) {
	cKey := C.CString(key)
	defer C.free(unsafe.Pointer(cKey))

	cResult := C.ketama_hasher_lookup(h.ptr, cKey)
	if cResult == nil {
		return "", false
	}
	defer C.ring_hasher_free_string(cResult)
	return C.GoString(cResult), true
}

func (h *KetamaHasher) Close() {
	if h.ptr != nil {
		C.ketama_hasher_destroy(h.ptr)
		h.ptr = nil
	}
}

// AnchorHasher implements Google's Anchor Hashing for minimal disruption
type AnchorHasher struct {
	ptr *C.AnchorHasher
}

// NewAnchorHasher creates a new Anchor hasher
func NewAnchorHasher() *AnchorHasher {
	h := &AnchorHasher{
		ptr: C.anchor_hasher_create(),
	}
	runtime.SetFinalizer(h, func(h *AnchorHasher) {
		C.anchor_hasher_destroy(h.ptr)
	})
	return h
}

func (h *AnchorHasher) AddNode(node Node) {
	cID := C.CString(node.ID)
	cAddress := C.CString(node.Address)
	cZone := C.CString(node.Zone)
	defer C.free(unsafe.Pointer(cID))
	defer C.free(unsafe.Pointer(cAddress))
	defer C.free(unsafe.Pointer(cZone))

	C.anchor_hasher_add_node(h.ptr, cID, cAddress, C.uint32_t(node.Weight), cZone)
}

func (h *AnchorHasher) RemoveNode(id string) {
	cID := C.CString(id)
	defer C.free(unsafe.Pointer(cID))
	C.anchor_hasher_remove_node(h.ptr, cID)
}

func (h *AnchorHasher) Lookup(key string) (string, bool) {
	cKey := C.CString(key)
	defer C.free(unsafe.Pointer(cKey))

	cResult := C.anchor_hasher_lookup(h.ptr, cKey)
	if cResult == nil {
		return "", false
	}
	defer C.ring_hasher_free_string(cResult)
	return C.GoString(cResult), true
}

func (h *AnchorHasher) Close() {
	if h.ptr != nil {
		C.anchor_hasher_destroy(h.ptr)
		h.ptr = nil
	}
}