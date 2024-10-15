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
