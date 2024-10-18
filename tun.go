package utun

/*
#include <stdio.h>
#include <stdlib.h>
#include "tun.h"
*/
import "C"
import (
	"fmt"
	"net"
	"os"
	"unsafe"
)

type Utun struct {
	fd   int
	file *os.File
	Name string
	MTU  int
}

const NOPEER = ""

func (u *Utun) SetIP(cidr string, peer string) error {
	ip, _, err := net.ParseCIDR(cidr)
	if err != nil {
		return err
	}

	name := C.CString(u.Name)
	defer func() { C.free(unsafe.Pointer(name)) }()

	csip := C.CString(ip.String())
	defer func() { C.free(unsafe.Pointer(csip)) }()

	csmask := C.CString("255.255.255.0")
	defer func() { C.free(unsafe.Pointer(csmask)) }()

	if peer == NOPEER {
		ret := C.configure_interface(name, csip, csmask, nil)
		if ret == -1 {
			return fmt.Errorf("setting interface address: %v", C.GoString(C.sys_error()))
		}
	} else {
		cspeer := C.CString(peer)
		defer func() { C.free(unsafe.Pointer(cspeer)) }()
		ret := C.configure_interface(name, csip, csmask, cspeer)
		if ret == -1 {
			return fmt.Errorf("setting interface address: %v", C.GoString(C.sys_error()))
		}
	}

	return nil
}

func (u *Utun) SetMTU(val int) error {
	name := C.CString(u.Name)
	defer func() { C.free(unsafe.Pointer(name)) }()

	ret := C.set_mtu(name, C.int(val))
	if ret == -1 {
		return fmt.Errorf("setting interface mtu: %v", C.GoString(C.sys_error()))
	}
	u.MTU = val
	return nil
}

func (u *Utun) Close() {
	u.file.Close()
}
