package main

import (
	"fmt"
	"log"
	"os"
	"strings"
	"github.com/Jeffail/gabs"
)

// Load and parse guest configuration from JSON format.
func loadGuestConfig(path string) (config *gabs.Container, err error) {
	src, err := os.Open(path);
	if err != nil {
		return
	}
	defer src.Close()
	config, err = gabs.ParseJSONBuffer(src)
	return
}

// Save guest configuration to path from JSON format.
func saveGuestConfig(config *gabs.Container, path string) (err error) {
	s := config.String()
	dst, err := os.Create(path)
	if err != nil {
		return
	}
	defer dst.Close()
	_, err = dst.WriteString(s)
	return
}

// Merge ("generate?") guest network configuration from host configuration.
func mergeNetConfig(netconfig *netConfig, guest *gabs.Container) (err error) {
	if guest.ExistsP("hostname") {
		log.Print("Guest already defines hostname, not overriden")
	} else {
		guest.SetP(netconfig.hostname, "hostname")
	}
	if guest.ExistsP("net") {
		return fmt.Errorf("Guest already defines net configuration")
	}
	addrs, _ := guest.ArrayOfSizeP(1, "net.interfaces.vioif0.addrs")
	addr, _ := addrs.ObjectI(0)
	addr.Set("inet", "type")
	addr.Set("static", "method")
	addr.Set(netconfig.ipAddress, "addr")
	gateways, _ := guest.ArrayOfSizeP(1, "net.gateways")
	gw, _ := gateways.ObjectI(0)
	gw.Set("inet", "type")
	gw.Set(netconfig.gateway, "addr")
	dns, _ := guest.ObjectP("net.dns")
	if netconfig.dnsServers != nil {
		dns.Set(netconfig.dnsServers, "nameservers")
	}
	if netconfig.dnsSearch != nil {
		dns.Set(netconfig.dnsSearch, "search")
	}
	return
}

// Merge guest volume/block device configuration with any volumes found in
// host container.
func mergeVolumeConfig(volumeconfig []volumeConfig, guest *gabs.Container) (err error) {
	var mounts *gabs.Container
	if guest.ExistsP("mount") {
		mounts = guest.S("mount")
	} else {
		mounts, _ = guest.ObjectP("mount")
	}
	mmap, _ := mounts.ChildrenMap()
	for _, mount := range mmap {
		var source string
		var path string
		var ok bool
		if source, ok = mount.Path("source").Data().(string); !ok {
			continue
		}
		if path, ok = mount.Path("path").Data().(string); !ok {
			continue
		}
		if source == "blk" && strings.HasPrefix(path, "/dev/ld") {
			return fmt.Errorf("Guest configuration defines " +
				"/dev/ld* block devices, cannot merge with " +
				"host volumes")
		}
	}
	blkIndex := 0
	for _, v := range volumeconfig {
		mp := "/" + v.name
		obj, _ := guest.Object("mount", mp)
		obj.Set("blk", "source")
		obj.Set(fmt.Sprintf("/dev/ld%va", blkIndex), "path")
		blkIndex += 1
	}
	return
}
