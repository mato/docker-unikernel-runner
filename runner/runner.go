package main

import (
    "fmt"
    "log"
    "os"
    "syscall"
    "github.com/Jeffail/gabs"
    "github.com/davecgh/go-spew/spew"
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
		// QEMU will be run as UID 1, ensure it can access /dev/kvm
		if err := syscall.Chown("/dev/kvm", 1, 1); err != nil {
			log.Fatalf("Could not chown() /dev/kvm: %v", err)
		}
	}
	// QEMU will be run as UID 1, ensure it can access /dev/net/tun.
	// Not strictly necessary as this should be mode 0666, but you never know.
	if err := syscall.Chown("/dev/net/tun", 1, 1); err != nil {
		log.Fatalf("Could not chown() /dev/net/tun: %v", err)
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
		// QEMU will be run as UID 1, ensure it can access the volume
		if err := syscall.Chown(v.path, 1, 1); err != nil {
			log.Fatalf("Could not chown() %v: %v", v.path, err)
		}
	}
	if verbose {
		fmt.Println("---- DEBUG: netConfig")
		spew.Dump(nCfg)
		fmt.Println("---- DEBUG: volumeConfig")
		spew.Dump(vCfg)
		fmt.Println("---- DEBUG: guestArgs")
		spew.Dump(guestArgs)
	}
	// So, there's no feasible way of dropping CAP_NET_ADMIN or calling
	// setuid() in a golang program (see https://github.com/golang/go/issues/1435)
	// Hence, we chown anything QEMU needs to write to to UID 1 above, and
	// make the qemu binary setuid.
	qemuPath := "/runtime/qemu/bin/qemu-system-x86_64"
	if err := syscall.Chown(qemuPath, 1, 1); err != nil {
		log.Fatalf("Could not chown() %v: %v", qemuPath, err)
	}
	if err := syscall.Chmod(qemuPath, 04755); err != nil {
		log.Fatalf("Could not chmod() %v: %v", qemuPath, err)
	}
	err = syscall.Exec(qemuPath, guestArgs, nil)
	if err != nil {
		log.Fatalf("Exec(%v, %v): %v", qemuPath, guestArgs, err)
	}
}
