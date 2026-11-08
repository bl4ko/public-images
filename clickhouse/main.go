package main

import (
	"os"
	"syscall"
)

const prSetTHPDisable = 41

func main() {
	if _, _, errno := syscall.RawSyscall6(syscall.SYS_PRCTL, prSetTHPDisable, 1, 0, 0, 0, 0); errno != 0 {
		os.Stderr.WriteString("prctl(PR_SET_THP_DISABLE): " + errno.Error() + "\n")
		os.Exit(1)
	}
	err := syscall.Exec("/usr/bin/clickhouse", os.Args, os.Environ())
	os.Stderr.WriteString("exec /usr/bin/clickhouse: " + err.Error() + "\n")
	os.Exit(1)
}
