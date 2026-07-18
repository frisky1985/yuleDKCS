package cache

import (
	"context"
	"testing"
	"time"

	"github.com/alicebob/miniredis/v2"
	"github.com/redis/go-redis/v9"
)

// Helper to create a miniredis-backed RedisCache for testing
func newTestCache(t *testing.T) (*RedisCache, *miniredis.Miniredis) {
	t.Helper()
	mr, err := miniredis.Run()
	if err != nil {
		t.Fatalf("failed to start miniredis: %v", err)
	}
	c := NewRedisCache(mr.Addr(), "", 0)
	return c, mr
}

func TestNewRedisCache(t *testing.T) {
	mr, err := miniredis.Run()
	if err != nil {
		t.Fatalf("failed to start miniredis: %v", err)
	}
	defer mr.Close()

	c := NewRedisCache(mr.Addr(), "", 0)
	if c == nil {
		t.Fatal("NewRedisCache returned nil")
	}
	if c.Client() == nil {
		t.Fatal("Client() returned nil")
	}
}

func TestPing(t *testing.T) {
	c, mr := newTestCache(t)
	defer mr.Close()

	ctx := context.Background()
	err := c.Ping(ctx)
	if err != nil {
		t.Fatalf("Ping failed: %v", err)
	}
}

func TestSetAndGet(t *testing.T) {
	c, mr := newTestCache(t)
	defer mr.Close()

	ctx := context.Background()
	key := "test:key"
	value := map[string]interface{}{
		"name": "test",
		"age":  30,
	}

	err := c.Set(ctx, key, value, time.Minute)
	if err != nil {
		t.Fatalf("Set failed: %v", err)
	}

	var result map[string]interface{}
	err = c.Get(ctx, key, &result)
	if err != nil {
		t.Fatalf("Get failed: %v", err)
	}

	if result["name"] != "test" {
		t.Errorf("expected name=test, got %v", result["name"])
	}
}

func TestGetNotFound(t *testing.T) {
	c, mr := newTestCache(t)
	defer mr.Close()

	ctx := context.Background()
	var result string
	err := c.Get(ctx, "nonexistent", &result)
	if err == nil {
		t.Fatal("expected error for nonexistent key")
	}
}

func TestGetBytes(t *testing.T) {
	c, mr := newTestCache(t)
	defer mr.Close()

	ctx := context.Background()
	err := c.Set(ctx, "key:bytes", "hello", time.Minute)
	if err != nil {
		t.Fatalf("Set failed: %v", err)
	}

	data, err := c.GetBytes(ctx, "key:bytes")
	if err != nil {
		t.Fatalf("GetBytes failed: %v", err)
	}
	if string(data) != `"hello"` {
		t.Errorf("expected %q,got %q", `"hello"`, string(data))
	}
}

func TestDelete(t *testing.T) {
	c, mr := newTestCache(t)
	defer mr.Close()

	ctx := context.Background()
	c.Set(ctx, "key:del", "value", time.Minute)
	err := c.Delete(ctx, "key:del")
	if err != nil {
		t.Fatalf("Delete failed: %v", err)
	}

	exists, err := c.Exists(ctx, "key:del")
	if err != nil {
		t.Fatalf("Exists failed: %v", err)
	}
	if exists {
		t.Error("expected key to be deleted")
	}
}

func TestExists(t *testing.T) {
	c, mr := newTestCache(t)
	defer mr.Close()

	ctx := context.Background()

	exists, err := c.Exists(ctx, "key:exists")
	if err != nil {
		t.Fatalf("Exists failed: %v", err)
	}
	if exists {
		t.Error("expected false for nonexistent key")
	}

	c.Set(ctx, "key:exists", "value", time.Minute)
	exists, err = c.Exists(ctx, "key:exists")
	if err != nil {
		t.Fatalf("Exists failed: %v", err)
	}
	if !exists {
		t.Error("expected true for existing key")
	}
}

func TestExpireAndTTL(t *testing.T) {
	c, mr := newTestCache(t)
	defer mr.Close()

	ctx := context.Background()
	c.Set(ctx, "key:ttl", "value", time.Hour)

	ttl, err := c.TTL(ctx, "key:ttl")
	if err != nil {
		t.Fatalf("TTL failed: %v", err)
	}
	if ttl <= 0 {
		t.Errorf("expected positive TTL, got %v", ttl)
	}

	err = c.Expire(ctx, "key:ttl", time.Minute)
	if err != nil {
		t.Fatalf("Expire failed: %v", err)
	}

	ttl, err = c.TTL(ctx, "key:ttl")
	if err != nil {
		t.Fatalf("TTL failed: %v", err)
	}
	if ttl > time.Hour {
		t.Errorf("expected TTL <= 1m, got %v", ttl)
	}
}

func TestIncrAndDecr(t *testing.T) {
	c, mr := newTestCache(t)
	defer mr.Close()

	ctx := context.Background()

	n, err := c.Incr(ctx, "counter:incr")
	if err != nil {
		t.Fatalf("Incr failed: %v", err)
	}
	if n != 1 {
		t.Errorf("expected 1, got %d", n)
	}

	n, err = c.IncrBy(ctx, "counter:incr", 5)
	if err != nil {
		t.Fatalf("IncrBy failed: %v", err)
	}
	if n != 6 {
		t.Errorf("expected 6, got %d", n)
	}

	n, err = c.Decr(ctx, "counter:incr")
	if err != nil {
		t.Fatalf("Decr failed: %v", err)
	}
	if n != 5 {
		t.Errorf("expected 5, got %d", n)
	}
}

func TestHSetAndHGet(t *testing.T) {
	c, mr := newTestCache(t)
	defer mr.Close()

	ctx := context.Background()
	hashKey := "hash:user:1"

	err := c.HSet(ctx, hashKey, "name", "Alice")
	if err != nil {
		t.Fatalf("HSet failed: %v", err)
	}

	var name string
	err = c.HGet(ctx, hashKey, "name", &name)
	if err != nil {
		t.Fatalf("HGet failed: %v", err)
	}
	if name != "Alice" {
		t.Errorf("expected Alice, got %s", name)
	}
}

func TestHGetAll(t *testing.T) {
	c, mr := newTestCache(t)
	defer mr.Close()

	ctx := context.Background()
	hashKey := "hash:config"

	c.HSet(ctx, hashKey, "max_conn", "100")
	c.HSet(ctx, hashKey, "timeout", "30s")
	c.HSet(ctx, hashKey, "debug", "true")

	all, err := c.HGetAll(ctx, hashKey)
	if err != nil {
		t.Fatalf("HGetAll failed: %v", err)
	}
	if len(all) != 3 {
		t.Errorf("expected 3 fields, got %d", len(all))
	}
}

func TestHDel(t *testing.T) {
	c, mr := newTestCache(t)
	defer mr.Close()

	ctx := context.Background()
	hashKey := "hash:test"
	c.HSet(ctx, hashKey, "field1", "v1")
	c.HSet(ctx, hashKey, "field2", "v2")

	err := c.HDel(ctx, hashKey, "field1")
	if err != nil {
		t.Fatalf("HDel failed: %v", err)
	}

	all, _ := c.HGetAll(ctx, hashKey)
	if len(all) != 1 {
		t.Errorf("expected 1 field after delete, got %d", len(all))
	}
}

func TestLPushAndLPop(t *testing.T) {
	c, mr := newTestCache(t)
	defer mr.Close()

	ctx := context.Background()
	listKey := "list:queue"

	err := c.LPush(ctx, listKey, "item1", "item2", "item3")
	if err != nil {
		t.Fatalf("LPush failed: %v", err)
	}

	var item string
	err = c.RPop(ctx, listKey, &item)
	if err != nil {
		t.Fatalf("RPop failed: %v", err)
	}

	llen, err := c.LLen(ctx, listKey)
	if err != nil {
		t.Fatalf("LLen failed: %v", err)
	}
	if llen != 2 {
		t.Errorf("expected length 2, got %d", llen)
	}
}

func TestRPushAndRPop(t *testing.T) {
	c, mr := newTestCache(t)
	defer mr.Close()

	ctx := context.Background()
	listKey := "list:queue2"

	err := c.RPush(ctx, listKey, "a", "b")
	if err != nil {
		t.Fatalf("RPush failed: %v", err)
	}

	rangeVals, err := c.LRange(ctx, listKey, 0, -1)
	if err != nil {
		t.Fatalf("LRange failed: %v", err)
	}
	if len(rangeVals) != 2 {
		t.Errorf("expected 2 items, got %d", len(rangeVals))
	}
}

func TestSAddAndSMembers(t *testing.T) {
	c, mr := newTestCache(t)
	defer mr.Close()

	ctx := context.Background()
	setKey := "set:tags"

	err := c.SAdd(ctx, setKey, "golang", "redis", "testing")
	if err != nil {
		t.Fatalf("SAdd failed: %v", err)
	}

	members, err := c.SMembers(ctx, setKey)
	if err != nil {
		t.Fatalf("SMembers failed: %v", err)
	}
	if len(members) != 3 {
		t.Errorf("expected 3 members, got %d", len(members))
	}
}

func TestSIsMember(t *testing.T) {
	c, mr := newTestCache(t)
	defer mr.Close()

	ctx := context.Background()
	setKey := "set:check"

	c.SAdd(ctx, setKey, "golang", "redis")

	isMember, err := c.SIsMember(ctx, setKey, "golang")
	if err != nil {
		t.Fatalf("SIsMember failed: %v", err)
	}
	if !isMember {
		t.Error("expected 'golang' to be a member")
	}

	isMember, err = c.SIsMember(ctx, setKey, "python")
	if err != nil {
		t.Fatalf("SIsMember failed: %v", err)
	}
	if isMember {
		t.Error("expected 'python' not to be a member")
	}
}

func TestSRem(t *testing.T) {
	c, mr := newTestCache(t)
	defer mr.Close()

	ctx := context.Background()
	setKey := "set:remove"
	c.SAdd(ctx, setKey, "a", "b", "c")

	err := c.SRem(ctx, setKey, "a")
	if err != nil {
		t.Fatalf("SRem failed: %v", err)
	}

	members, _ := c.SMembers(ctx, setKey)
	if len(members) != 2 {
		t.Errorf("expected 2 members after remove, got %d", len(members))
	}
}

func TestZAddAndZRange(t *testing.T) {
	c, mr := newTestCache(t)
	defer mr.Close()

	ctx := context.Background()
	zsetKey := "zset:scores"

	err := c.ZAdd(ctx, zsetKey, 95.5, "Alice")
	if err != nil {
		t.Fatalf("ZAdd failed: %v", err)
	}
	err = c.ZAdd(ctx, zsetKey, 87.0, "Bob")
	if err != nil {
		t.Fatalf("ZAdd failed: %v", err)
	}
	err = c.ZAdd(ctx, zsetKey, 91.0, "Charlie")
	if err != nil {
		t.Fatalf("ZAdd failed: %v", err)
	}

	results, err := c.ZRange(ctx, zsetKey, "85", "100", 0, 10)
	if err != nil {
		t.Fatalf("ZRange failed: %v", err)
	}
	if len(results) == 0 {
		t.Error("expected at least 1 result")
	}
}

func TestZRemRangeByScore(t *testing.T) {
	c, mr := newTestCache(t)
	defer mr.Close()

	ctx := context.Background()
	zsetKey := "zset:cleanup"
	c.ZAdd(ctx, zsetKey, 10, "old")
	c.ZAdd(ctx, zsetKey, 20, "new")

	err := c.ZRemRangeByScore(ctx, zsetKey, "-inf", "15")
	if err != nil {
		t.Fatalf("ZRemRangeByScore failed: %v", err)
	}

	results, _ := c.ZRange(ctx, zsetKey, "-inf", "+inf", 0, 10)
	if len(results) != 1 {
		t.Errorf("expected 1 remaining member, got %d", len(results))
	}
}

func TestLockAndUnlock(t *testing.T) {
	c, mr := newTestCache(t)
	defer mr.Close()

	ctx := context.Background()
	lockKey := "lock:resource"

	acquired, err := c.Lock(ctx, lockKey, time.Second)
	if err != nil {
		t.Fatalf("Lock failed: %v", err)
	}
	if !acquired {
		t.Fatal("expected to acquire lock")
	}

	// Second attempt should fail
	acquired, err = c.Lock(ctx, lockKey, time.Second)
	if err != nil {
		t.Fatalf("Lock failed: %v", err)
	}
	if acquired {
		t.Error("expected second lock attempt to fail")
	}

	// Unlock
	err = c.Unlock(ctx, lockKey)
	if err != nil {
		t.Fatalf("Unlock failed: %v", err)
	}

	// Can acquire again after unlock
	acquired, err = c.Lock(ctx, lockKey, time.Second)
	if err != nil {
		t.Fatalf("Lock failed: %v", err)
	}
	if !acquired {
		t.Error("expected to re-acquire lock after unlock")
	}
}

func TestTryLock(t *testing.T) {
	c, mr := newTestCache(t)
	defer mr.Close()

	ctx := context.Background()
	lockKey := "lock:try"

	// Should succeed on first attempt
	acquired, err := c.TryLock(ctx, lockKey, time.Second, 10*time.Millisecond, 3)
	if err != nil {
		t.Fatalf("TryLock failed: %v", err)
	}
	if !acquired {
		t.Fatal("expected to acquire lock on first try")
	}

	// Another lock should fail (with retries)
	acquired, err = c.TryLock(ctx, lockKey, time.Second, 10*time.Millisecond, 3)
	if err != nil {
		t.Fatalf("TryLock failed: %v", err)
	}
	if acquired {
		t.Error("expected TryLock to fail when lock is held")
	}
}

func TestClose(t *testing.T) {
	c, mr := newTestCache(t)
	defer mr.Close()

	err := c.Close()
	if err != nil {
		t.Fatalf("Close failed: %v", err)
	}
}

func TestSetWithStruct(t *testing.T) {
	c, mr := newTestCache(t)
	defer mr.Close()

	ctx := context.Background()
	type User struct {
		ID   int    `json:"id"`
		Name string `json:"name"`
	}
	user := User{ID: 1, Name: "Alice"}

	err := c.Set(ctx, "user:1", user, time.Minute)
	if err != nil {
		t.Fatalf("Set failed: %v", err)
	}

	var result User
	err = c.Get(ctx, "user:1", &result)
	if err != nil {
		t.Fatalf("Get failed: %v", err)
	}
	if result.Name != "Alice" {
		t.Errorf("expected Alice, got %s", result.Name)
	}
}

func TestHSetWithStruct(t *testing.T) {
	c, mr := newTestCache(t)
	defer mr.Close()

	ctx := context.Background()
	type Config struct {
		Host string `json:"host"`
		Port int    `json:"port"`
	}
	cfg := Config{Host: "localhost", Port: 8080}

	err := c.HSet(ctx, "hash:config2", "server", cfg)
	if err != nil {
		t.Fatalf("HSet failed: %v", err)
	}

	var result Config
	err = c.HGet(ctx, "hash:config2", "server", &result)
	if err != nil {
		t.Fatalf("HGet failed: %v", err)
	}
	if result.Host != "localhost" {
		t.Errorf("expected localhost, got %s", result.Host)
	}
}

func TestNewRedisCacheFromClient(t *testing.T) {
	_, mr := newTestCache(t)
	defer mr.Close()

	client := redis.NewClient(&redis.Options{Addr: mr.Addr()})
	defer client.Close()

	c := NewRedisCacheFromClient(client)
	if c == nil {
		t.Fatal("NewRedisCacheFromClient returned nil")
	}
	if c.Client() != client {
		t.Error("expected same client reference")
	}

	ctx := context.Background()
	err := c.Ping(ctx)
	if err != nil {
		t.Fatalf("Ping failed: %v", err)
	}
}

func TestSet_MarshalError(t *testing.T) {
	c, mr := newTestCache(t)
	defer mr.Close()

	ctx := context.Background()
	err := c.Set(ctx, "key:bad", make(chan int), time.Minute)
	if err == nil {
		t.Fatal("expected marshal error for channel value")
	}
}

func TestHSet_MarshalError(t *testing.T) {
	c, mr := newTestCache(t)
	defer mr.Close()

	ctx := context.Background()
	err := c.HSet(ctx, "hash:bad", "field", func() {})
	if err == nil {
		t.Fatal("expected marshal error for function value")
	}
}

func TestLPop(t *testing.T) {
	c, mr := newTestCache(t)
	defer mr.Close()

	ctx := context.Background()
	listKey := "list:lpop-test"

	err := c.LPush(ctx, listKey, "third", "second", "first")
	if err != nil {
		t.Fatalf("LPush failed: %v", err)
	}

	var item string
	err = c.LPop(ctx, listKey, &item)
	if err != nil {
		t.Fatalf("LPop failed: %v", err)
	}
	if item != "first" {
		t.Errorf("expected 'first', got %s", item)
	}

	// Pop twice more to exhaust the list
	c.LPop(ctx, listKey, &item)
	c.LPop(ctx, listKey, &item)

	// Pop from empty list should fail
	err = c.LPop(ctx, listKey, &item)
	if err == nil {
		t.Error("expected error when popping from empty list")
	}
}

func TestRPop_Error(t *testing.T) {
	c, mr := newTestCache(t)
	defer mr.Close()

	ctx := context.Background()
	listKey := "list:rpop-test"

	c.RPush(ctx, listKey, "a", "b")

	var item string
	err := c.RPop(ctx, listKey, &item)
	if err != nil {
		t.Fatalf("RPop failed on populated list: %v", err)
	}
	if item != "b" {
		t.Errorf("expected 'b', got %s", item)
	}

	c.RPop(ctx, listKey, &item)

	err = c.RPop(ctx, listKey, &item)
	if err == nil {
		t.Error("expected error when popping from empty list")
	}
}

func TestHGet_Error(t *testing.T) {
	c, mr := newTestCache(t)
	defer mr.Close()

	ctx := context.Background()

	var result string
	err := c.HGet(ctx, "hash:nonexistent", "field", &result)
	if err == nil {
		t.Error("expected error for non-existent hash field")
	}
}

func TestTryLock_Error(t *testing.T) {
	c, mr := newTestCache(t)
	defer mr.Close()

	ctx := context.Background()

	// Close the underlying Redis client to force errors
	c.client.Close()

	_, err := c.TryLock(ctx, "lock:err", time.Second, time.Millisecond, 2)
	if err == nil {
		t.Error("expected error from closed redis connection")
	}
}


