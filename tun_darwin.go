// build +darwin
package utun

/*
#cgo CFLAGS: -Wall
#cgo LDFLAGS: -framework CoreFoundation -framework SystemConfiguration
#include <stdlib.h>
#include <unistd.h>
#include "tun.h"
*/
import "C"
import (
	"fmt"
	"os"

	"golang.org/x/sys/unix"
)

func OpenUtun() (*Utun, error) {
	var u Utun
	fd := C.open_utun()
	if fd == -1 {
		err := C.GoString(C.sys_error())
		return nil, fmt.Errorf("at open utun: %v", err)
	}
	u.fd = int(fd)
	u.file = os.NewFile(uintptr(u.fd), "utun")

	name := C.get_utun_name(fd)
	if name == nil {
		C.close(fd)
		err := C.GoString(C.sys_error())
		return nil, fmt.Errorf("get utun name: %v", err)
	}
	u.Name = C.GoString(name)
	return &u, nil
}

func (u *Utun) Read(buf []byte) (int, error) {
	var tmp [2048]byte

	if len(buf) < u.MTU {
		return 0, fmt.Errorf("invalid buf len, less than MTU")
	}
	for {
		n, e := u.file.Read(tmp[:u.MTU+4])
		fmt.Println("Tun inside tun read", n, e)
		if e != nil {
			return 0, e
		}
		if n > 4 {
			if tmp[3] == unix.AF_INET {
				copy(buf, tmp[4:n])
				return n - 4, nil
			}
		}
	}
}

func (u *Utun) Write(buf []byte) (int, error) {
	var tmp [2048]byte
	if len(buf) > u.MTU {
		return 0, fmt.Errorf("invalid buf len, greather than MTU")
	}
	tmp[0] = 0x00
	tmp[1] = 0x00
	tmp[2] = 0x00
	tmp[3] = unix.AF_INET
	copy(tmp[4:], buf)
	return u.file.Write(tmp[0 : len(buf)+4])
}

func (u *Utun) Close() {
	u.file.Close()
	unix.Close(u.fd)
}
