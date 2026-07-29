package relay

import (
	"time"
)

// ExpiryManager 管理邮箱 TTL 过期自动清理
type ExpiryManager struct {
	controller *MailboxController
	interval   time.Duration
	stopCh     chan struct{}
}

func NewExpiryManager(controller *MailboxController, interval time.Duration) *ExpiryManager {
	if interval <= 0 {
		interval = 5 * time.Minute
	}
	return &ExpiryManager{
		controller: controller,
		interval:   interval,
		stopCh:     make(chan struct{}),
	}
}

// Start 启动后台过期扫描协程
func (m *ExpiryManager) Start() {
	go func() {
		ticker := time.NewTicker(m.interval)
		defer ticker.Stop()

		for {
			select {
			case <-ticker.C:
				expired := m.controller.ExpireScan()
				if expired > 0 {
					// log already handled in controller
				}
			case <-m.stopCh:
				return
			}
		}
	}()
}

// Stop 停止过期扫描
func (m *ExpiryManager) Stop() {
	close(m.stopCh)
}
