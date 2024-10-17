// build +linux
package utun

/*
#cgo CFLAGS: -Wall
#include <stdlib.h>
#include <unistd.h>
#include "tun.h"
*/
import "C"
import (
	"fmt"
	"os"
	"unsafe"
)

func OpenUtun() (*Utun, error) {
	var u Utun

	dev := C.alloc_ifname()
	if dev == nil {
		err := C.GoString(C.sys_error())
		return nil, fmt.Errorf("alloc ifname: %v", err)
	}

	defer func() { C.free(unsafe.Pointer(dev)) }()
	fd := C.tun_alloc(dev)
	if fd == -1 {
		err := C.GoString(C.sys_error())
		return nil, fmt.Errorf("alloc ifname: %v", err)
	}

	u.fd = int(fd)
	u.file = os.NewFile(uintptr(u.fd), "utun")
	u.Name = C.GoString(dev)
	return &u, nil
}

func (u *Utun) Read(buf []byte) (int, error) {
	if len(buf) < u.MTU {
		return 0, fmt.Errorf("invalid buf len, less than MTU")
	}
	return u.file.Read(buf[:u.MTU])
}

func (u *Utun) Write(buf []byte) (int, error) {
	if len(buf) > u.MTU {
		return 0, fmt.Errorf("invalid buf len, greather than MTU")
	}
	return u.file.Write(buf)
}
