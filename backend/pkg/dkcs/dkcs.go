// Package dkcs 提供 yuleDKCS C++ 库的 Go 绑定
// 注: 需要先编译 yuleDKCS C++ 库为共享库
package dkcs

/*
#cgo LDFLAGS: -lyuledkcs -L${SRCDIR}/lib
#include <stdlib.h>

// TODO: 添加 C++ 库的头文件声明
// extern int dkcs_init();
// extern void dkcs_cleanup();
*/
import "C"
import (
	"fmt"
	"unsafe"
)

// 全局状态
var initialized bool

// Init 初始化 yuleDKCS 库
func Init() error {
	if initialized {
		return nil
	}

	// TODO: 调用 C++ 初始化函数
	// ret := C.dkcs_init()
	// if ret != 0 {
	//     return fmt.Errorf("failed to initialize dkcs: %d", ret)
	// }

	initialized = true
	return nil
}

// Cleanup 清理 yuleDKCS 库
func Cleanup() error {
	if !initialized {
		return nil
	}

	// TODO: 调用 C++ 清理函数
	// C.dkcs_cleanup()

	initialized = false
	return nil
}

// Version 返回版本信息
func Version() string {
	// TODO: 从 C++ 库获取版本
	return "yuleDKCS Go Binding v0.1.0"
}

// IsInitialized 检查是否已初始化
func IsInitialized() bool {
	return initialized
}

// cString 辅助函数: Go string 转 C string
func cString(s string) *C.char {
	return C.CString(s)
}

// goString 辅助函数: C string 转 Go string
func goString(s *C.char) string {
	return C.GoString(s)
}

// freeCString 辅助函数: 释放 C string
func freeCString(s *C.char) {
	C.free(unsafe.Pointer(s))
}

// TODO: 添加更多 yuleDKCS 功能绑定
// - 验证功能
// - 密钥管理
// - 加解密操作