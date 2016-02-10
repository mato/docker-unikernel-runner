package main

import (
    "fmt"
    "log"
    "os"
    "syscall"
    "github.com/Jeffail/gabs"
    "github.com/davecgh/go-spew/spew"
//    "github.com/syndtr/gocapability/capability"
)

func main() {
	// User-set, be verbose?
	verbose := (os.Getenv("RUNNER_VERBOSE") == "1")
	// Determine if KVM is available.
	var hasKvm bool
	devKvm, err := os.OpenFile("/dev/kvm", os.O_RDWR, 0)
	if err != nil {
		hasKvm = false
	} else {
		hasKvm = true
		devKvm.Close()
	}
	// Load any configuration provided with the guest, or create an empty
	// configuration if none.
	gCfg, err := loadGuestConfig("/unikernel/config.json")
	if err != nil {
		if os.IsNotExist(err) {
			gCfg = gabs.New()
		} else {
			log.Fatal(err)
		}
	}
	// Detect container-side ("host") network configuration and merge with
	// guest configuration.
	nCfg, err := getNetConfig()
	if err != nil {
		log.Fatalf("getNetConfig(): %v", err)
	}
	if err := mergeNetConfig(nCfg, gCfg); err != nil {
		log.Fatalf("mergeNetConfig(): %v", err)
	}
	// Enumerate volumes in /unikernel/fs/* (if any) and merge with guest
	// block device configuration.
	vCfg, err := getVolumeConfig("/unikernel/fs")
	if err != nil {
		log.Fatalf("getVolumeConfig(): %v", err)
	}
	if err := mergeVolumeConfig(vCfg, gCfg); err != nil {
		log.Fatalf("mergeVolumeConfig(): %v", err)
	}
	// Save merged guest configuration to /unikernel/run.json.
	if err := saveGuestConfig(gCfg, "/unikernel/run.json"); err != nil {
		log.Fatalf("saveGuestConfig(): %v", err)
	}
	// Wire up guest tap interface.
	tapInterface := wireTapInterface(nCfg)
	// Construct qemu arguments.
	guestArgs := []string{
		"qemu-system-x86_64",
		"-kernel", "/unikernel/unikernel.bin",
		"-initrd", "/unikernel/run.json",
		"-vga", "none", "-nographic",
	}
	// Enable KVM if available
	if hasKvm {
		guestArgs = append(guestArgs,
			"-enable-kvm", "-cpu", "host,migratable=no,+invtsc")
	}
	// Tap interface -> virtio-net
	guestArgs = append(guestArgs, "-net")
	guestArgs = append(guestArgs,
		fmt.Sprintf("nic,macaddr=%s,model=virtio",
			nCfg.hardwareAddress))
	guestArgs = append(guestArgs, "-net")
	guestArgs = append(guestArgs,
		fmt.Sprintf("tap,ifname=%s,script=no,downscript=no",
			tapInterface))
	// Block devices -> virtio-block
	for _, v := range vCfg {
		guestArgs = append(guestArgs, "-drive")
		volArg := fmt.Sprintf("file=%v,if=virtio,format=raw", v.path)
		guestArgs = append(guestArgs, volArg)
	}
	if verbose {
		fmt.Println("---- DEBUG: netConfig")
		spew.Dump(nCfg)
		fmt.Println("---- DEBUG: volumeConfig")
		spew.Dump(vCfg)
		fmt.Println("---- DEBUG: guestArgs")
		spew.Dump(guestArgs)
	}
	// Drop CAP_NET_ADMIN here, we no longer need it.
	// XXX This doesn't actually appear to do anything, "getpcaps" on the
	// qemu process still prints "cap_net_admin+ep" in its list?!
	// caps, err := capability.NewPid(0)
//	if err != nil {
//		log.Fatalf("Init capabilities: %v", err)
//	}
//	if err := caps.Load(); err != nil {
//		log.Fatalf("Get capabilities: %v", err)
//	}
//	caps.Unset(capability.INHERITABLE | capability.EFFECTIVE | capability.PERMITTED | capability.BOUNDING, capability.CAP_NET_ADMIN)
//	if err := caps.Apply(capability.CAPS); err != nil {
//		log.Fatalf("Cannot drop CAP_NET_ADMIN: %v", err)
//	}
	// Boom!
	err = syscall.Exec("/runtime/qemu/bin/qemu-system-x86_64", guestArgs, nil)
	if err != nil {
		log.Fatalf("Exec(%v): %v", guestArgs, err)
	}
}
