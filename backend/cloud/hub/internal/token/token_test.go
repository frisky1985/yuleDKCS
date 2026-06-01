package token

import (
	"testing"
	"time"
)

func TestIssueAndVerify(t *testing.T) {
	svc := NewService("test-secret")

	tok, err := svc.Issue("车主A", "代驾公司", "VIN123",
		[]Permission{PermLock, PermEngineStart},
		2*time.Hour, 0)

	if err != nil {
		t.Fatalf("Issue failed: %v", err)
	}
	if tok.ID == "" {
		t.Fatal("expected non-empty token ID")
	}
	if tok.Signature == "" {
		t.Fatal("expected non-empty signature")
	}
	if tok.Status != "active" {
		t.Fatalf("expected active, got %s", tok.Status)
	}

	// Verify
	verified, err := svc.Verify(tok.ID)
	if err != nil {
		t.Fatalf("Verify failed: %v", err)
	}
	if verified.SubjectID != "代驾公司" {
		t.Fatalf("expected 代驾公司, got %s", verified.SubjectID)
	}
}

func TestExpiredToken(t *testing.T) {
	svc := NewService("test-secret")

	tok, _ := svc.Issue("车主A", "某人", "VIN123",
		[]Permission{PermLock}, 1*time.Millisecond, 0)

	time.Sleep(10 * time.Millisecond)

	_, err := svc.Verify(tok.ID)
	if err == nil {
		t.Fatal("expected error for expired token")
	}
}

func TestRevokedToken(t *testing.T) {
	svc := NewService("test-secret")

	tok, _ := svc.Issue("车主A", "某人", "VIN123",
		[]Permission{PermLock}, 1*time.Hour, 0)

	svc.Revoke(tok.ID, "车主A")

	_, err := svc.Verify(tok.ID)
	if err == nil {
		t.Fatal("expected error for revoked token")
	}
}

func TestWrongOwnerCannotRevoke(t *testing.T) {
	svc := NewService("test-secret")

	tok, _ := svc.Issue("车主A", "某人", "VIN123",
		[]Permission{PermLock}, 1*time.Hour, 0)

	err := svc.Revoke(tok.ID, "攻击者")
	if err == nil {
		t.Fatal("expected error: wrong owner should not be able to revoke")
	}
}

func TestMaxUses(t *testing.T) {
	svc := NewService("test-secret")

	tok, _ := svc.Issue("车主A", "某人", "VIN123",
		[]Permission{PermLock}, 1*time.Hour, 3)

	// Use 3 times
	for i := 0; i < 3; i++ {
		_, err := svc.Verify(tok.ID)
		if err != nil {
			t.Fatalf("verify attempt %d failed: %v", i+1, err)
		}
	}

	// 4th should fail
	_, err := svc.Verify(tok.ID)
	if err == nil {
		t.Fatal("expected error after max uses exceeded")
	}
}

func TestTamperedToken(t *testing.T) {
	svc := NewService("test-secret")

	tok, _ := svc.Issue("车主A", "某人", "VIN123",
		[]Permission{PermLock}, 1*time.Hour, 0)

	// Tamper with signature
	tok.Signature = "fake"

	_, err := svc.Verify(tok.ID)
	if err == nil {
		t.Fatal("expected error for tampered signature")
	}
}

func TestMultiplePermissions(t *testing.T) {
	svc := NewService("test-secret")

	perms := []Permission{PermLock, PermEngineStart, PermTrunk, PermClimate}
	tok, _ := svc.Issue("车主A", "家人", "VIN123", perms, 24*time.Hour, 0)

	verified, _ := svc.Verify(tok.ID)
	if len(verified.Perms) != 4 {
		t.Fatalf("expected 4 permissions, got %d", len(verified.Perms))
	}
}

func TestTamperedPermissions(t *testing.T) {
	svc := NewService("test-secret")

	tok, _ := svc.Issue("车主A", "某人", "VIN123", []Permission{PermLock}, 1*time.Hour, 0)

	// 篡改权限：把 lock 改成 full access
	tok.Perms = []Permission{PermLock, PermEngineStart, PermTrunk, PermClimate, PermSeat}

	// Verify 应该拒绝
	_, err := svc.Verify(tok.ID)
	if err == nil {
		t.Fatal("expected error for tampered permissions")
	}
}

func TestSuspendResume(t *testing.T) {
	svc := NewService("test-secret")

	tok, _ := svc.Issue("车主A", "某人", "VIN123", []Permission{PermLock}, 1*time.Hour, 0)

	// 挂起
	if err := svc.Suspend(tok.ID, "车主A"); err != nil {
		t.Fatalf("Suspend failed: %v", err)
	}

	// 挂起后验证应失败
	if _, err := svc.Verify(tok.ID); err == nil {
		t.Fatal("expected error for suspended token")
	}

	// 恢复
	if err := svc.Resume(tok.ID, "车主A"); err != nil {
		t.Fatalf("Resume failed: %v", err)
	}

	// 恢复后应能验证
	if _, err := svc.Verify(tok.ID); err != nil {
		t.Fatalf("Verify after resume failed: %v", err)
	}
}

func TestListByOwner(t *testing.T) {
	svc := NewService("test-secret")

	svc.Issue("车主A", "某人1", "VIN001", []Permission{PermLock}, 1*time.Hour, 0)
	svc.Issue("车主A", "某人2", "VIN002", []Permission{PermLock}, 1*time.Hour, 0)
	svc.Issue("车主B", "某人3", "VIN003", []Permission{PermLock}, 1*time.Hour, 0)

	tokens := svc.ListByOwner("车主A")
	if len(tokens) != 2 {
		t.Fatalf("expected 2 tokens for 车主A, got %d", len(tokens))
	}
}
