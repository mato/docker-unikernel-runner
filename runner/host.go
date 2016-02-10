package main

import (
	"crypto/rand"
	"fmt"
	"log"
	"net"
	"os"
	"strings"
	"syscall"
	"github.com/vishvananda/netlink"
)

// Host-side network configuration.
type netConfig struct {
	hostname string
	ipAddress string
	gateway string
	hardwareAddress string
	dnsServers []string
	dnsSearch []string
}

// Detect and return host-side network configuration.
func getNetConfig() (config *netConfig, err error) {
	eth0, err := netlink.LinkByName("eth0")
	if err != nil {
		return nil, fmt.Errorf("LinkByName(eth0): %v", err)
	}
	eth0Addrs, err := netlink.AddrList(eth0, syscall.AF_INET)
	if err != nil {
		return nil, fmt.Errorf("AddrList(eth0): %v", err)
	}
	if len(eth0Addrs) != 1 {
		return nil, fmt.Errorf("eth0: Expected single IPv4 address")
	}
	// TODO Is there a better way than relying on "8.8.8.8" being past
	// the default router?
	defaultroute, err := netlink.RouteGet(net.ParseIP("8.8.8.8"))
	if len(defaultroute) != 1 {
		return nil, fmt.Errorf("Could not determine single default route (got %v)", len(defaultroute))
	}
	eth0Attrs := eth0.Attrs()
	dns := dnsReadConfig("/etc/resolv.conf")
	hostname, _ := os.Hostname()
	config = &netConfig{
		hostname,
		eth0Addrs[0].IPNet.String(),
		defaultroute[0].Gw.String(),
		eth0Attrs.HardwareAddr.String(),
		dns.servers,
		dns.search,
	}
	return
}

// Flush all L3 addresses on link.
func flushAddresses(link netlink.Link) (err error) {
	addrs, err := netlink.AddrList(link, 0)
	if err != nil {
		return err
	}
	for _, addr := range addrs {
		if err = netlink.AddrDel(link, &addr); err != nil {
			return err
		}
	}
	return
}

// Generate a random, locally assigned and unicast MAC address.
func generateHardwareAddr() (addr net.HardwareAddr) {
	addr = make(net.HardwareAddr, 6)
	_, err := rand.Read(addr)
	if err != nil {
		log.Fatalf("Could not get random data: %v", err)
	}
	addr[0] &= 0xfe
	addr[0] |= 0x02
	return
}

// Wire up a tap interface for communicating with the guest. Returns the name
// of the created tap interface.
func wireTapInterface(config *netConfig) (string) {
	// Drop link on eth0 before configuring anything
	eth0, err := netlink.LinkByName("eth0")
	if err != nil {
		log.Fatalf("LinkByName(eth0): %v", err)
	}
	if err := netlink.LinkSetDown(eth0); err != nil {
		log.Fatalf("LinkSetDown(eth0): %v", err)
	}
	// Flush any L3 addresses on eth0
	if err := flushAddresses(eth0); err != nil {
		log.Fatalf("flushAddresses(eth0): %v", err)
	}
	// Generate and set random MAC address for eth0
	eth0Addr := generateHardwareAddr()
	if err := netlink.LinkSetHardwareAddr(eth0, eth0Addr); err != nil {
		log.Fatalf("LinkSetHardwareAddr(eth0): %v", err)
	}
	// Create "tap0" (interface to guest)
	tap0Attrs := netlink.NewLinkAttrs()
	tap0Attrs.Name = "tap0"
	tap0 := &netlink.Tuntap{tap0Attrs, netlink.TUNTAP_MODE_TAP}
	if err := netlink.LinkAdd(tap0); err != nil {
		log.Fatalf("LinkAdd(tap0): %v", err)
	}
	// Create a new bridge, br0 and add eth0 and tap0 to it
	br0Attrs := netlink.NewLinkAttrs()
	br0Attrs.Name = "br0"
	br0 := &netlink.Bridge{br0Attrs}
	if err := netlink.LinkAdd(br0); err != nil {
		log.Fatalf("LinkAdd(br0): %v", err)
	}
	if err := netlink.LinkSetMaster(eth0, br0); err != nil {
		log.Fatalf("LinkSetMaster(eth0, br0): %v", err)
	}
	if err := netlink.LinkSetMaster(tap0, br0); err != nil {
		log.Fatalf("LinkSetMaster(tap0, br0): %v", err)
	}
	// Set all links up
	if err := netlink.LinkSetUp(tap0); err != nil {
		log.Fatalf("LinkSetUp(tap0): %v", err)
	}
	if err := netlink.LinkSetUp(eth0); err != nil {
		log.Fatalf("LinkSetUp(eth0): %v", err)
	}
	if err := netlink.LinkSetUp(br0); err != nil {
		log.Fatalf("LinkSetUp(br0): %v", err)
	}
	return tap0Attrs.Name
}

// Host-side volume/block device configuration.
type volumeConfig struct {
	name string // Basename of volume
	path string // Full path to image file
}

// Enumerate and return host-side volume images, if any.
func getVolumeConfig(path string) (volumes []volumeConfig, err error) {
	dir, err := os.Open(path)
	if err != nil {
		if os.IsNotExist(err) {
			return nil, nil
		} else {
			return nil, err
		}
	}
	defer dir.Close()
	fi, err := dir.Readdir(-1)
	if err != nil {
		return nil, fmt.Errorf("Readdir(%v): %v", path, err)
	}
	for _, f := range fi {
		// Consider only plain files, and strip any (last) extension.
		if !f.IsDir() {
			name := f.Name()
			end := strings.LastIndex(name, ".")
			if end != -1 {
				name = name[:end]
			}
			var v volumeConfig
			v.name = name
			v.path = path + "/" + f.Name()
			volumes = append(volumes, v)
		}
	}
	return
}
