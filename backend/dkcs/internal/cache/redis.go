package cache

import (
	"context"
	"encoding/json"
	"fmt"
	"time"

	"github.com/redis/go-redis/v9"
)

// RedisCache wraps Redis client with helper methods
type RedisCache struct {
	client *redis.Client
}

// NewRedisCache creates a new RedisCache
func NewRedisCache(addr, password string, db int) *RedisCache {
	client := redis.NewClient(&redis.Options{
		Addr:     addr,
		Password: password,
		DB:       db,
	})
	return &RedisCache{client: client}
}

// Client returns the underlying Redis client
func (c *RedisCache) Client() *redis.Client {
	return c.client
}

// Set stores a key-value pair with expiration
func (c *RedisCache) Set(ctx context.Context, key string, value interface{}, expiration time.Duration) error {
	data, err := json.Marshal(value)
	if err != nil {
		return fmt.Errorf("failed to marshal value: %w", err)
	}
	return c.client.Set(ctx, key, data, expiration).Err()
}

// Get retrieves a value by key
func (c *RedisCache) Get(ctx context.Context, key string, dest interface{}) error {
	data, err := c.client.Get(ctx, key).Bytes()
	if err != nil {
		return err
	}
	return json.Unmarshal(data, dest)
}

// GetBytes retrieves raw bytes by key
func (c *RedisCache) GetBytes(ctx context.Context, key string) ([]byte, error) {
	return c.client.Get(ctx, key).Bytes()
}

// Delete deletes a key
func (c *RedisCache) Delete(ctx context.Context, key string) error {
	return c.client.Del(ctx, key).Err()
}

// Exists checks if key exists
func (c *RedisCache) Exists(ctx context.Context, key string) (bool, error) {
	n, err := c.client.Exists(ctx, key).Result()
	return n > 0, err
}

// Expire sets expiration for a key
func (c *RedisCache) Expire(ctx context.Context, key string, expiration time.Duration) error {
	return c.client.Expire(ctx, key, expiration).Err()
}

// TTL returns remaining TTL for a key
func (c *RedisCache) TTL(ctx context.Context, key string) (time.Duration, error) {
	return c.client.TTL(ctx, key).Result()
}

// Incr increments a counter
func (c *RedisCache) Incr(ctx context.Context, key string) (int64, error) {
	return c.client.Incr(ctx, key).Result()
}

// IncrBy increments by delta
func (c *RedisCache) IncrBy(ctx context.Context, key string, delta int64) (int64, error) {
	return c.client.IncrBy(ctx, key, delta).Result()
}

// Decr decrements a counter
func (c *RedisCache) Decr(ctx context.Context, key string) (int64, error) {
	return c.client.Decr(ctx, key).Result()
}

// HSet sets hash field
func (c *RedisCache) HSet(ctx context.Context, key string, field string, value interface{}) error {
	data, err := json.Marshal(value)
	if err != nil {
		return err
	}
	return c.client.HSet(ctx, key, field, data).Err()
}

// HGet gets hash field
func (c *RedisCache) HGet(ctx context.Context, key, field string, dest interface{}) error {
	data, err := c.client.HGet(ctx, key, field).Bytes()
	if err != nil {
		return err
	}
	return json.Unmarshal(data, dest)
}

// HGetAll gets all hash fields
func (c *RedisCache) HGetAll(ctx context.Context, key string) (map[string]string, error) {
	return c.client.HGetAll(ctx, key).Result()
}

// HDel deletes hash field
func (c *RedisCache) HDel(ctx context.Context, key string, fields ...string) error {
	return c.client.HDel(ctx, key, fields...).Err()
}

// LPush pushes to list head
func (c *RedisCache) LPush(ctx context.Context, key string, values ...interface{}) error {
	args := make([]interface{}, len(values))
	for i, v := range values {
		data, _ := json.Marshal(v)
		args[i] = data
	}
	return c.client.LPush(ctx, key, args...).Err()
}

// RPush pushes to list tail
func (c *RedisCache) RPush(ctx context.Context, key string, values ...interface{}) error {
	args := make([]interface{}, len(values))
	for i, v := range values {
		data, _ := json.Marshal(v)
		args[i] = data
	}
	return c.client.RPush(ctx, key, args...).Err()
}

// LPop pops from list head
func (c *RedisCache) LPop(ctx context.Context, key string, dest interface{}) error {
	data, err := c.client.LPop(ctx, key).Bytes()
	if err != nil {
		return err
	}
	return json.Unmarshal(data, dest)
}

// RPop pops from list tail
func (c *RedisCache) RPop(ctx context.Context, key string, dest interface{}) error {
	data, err := c.client.RPop(ctx, key).Bytes()
	if err != nil {
		return err
	}
	return json.Unmarshal(data, dest)
}

// LRange gets list range
func (c *RedisCache) LRange(ctx context.Context, key string, start, stop int64) ([]string, error) {
	return c.client.LRange(ctx, key, start, stop).Result()
}

// LLen returns list length
func (c *RedisCache) LLen(ctx context.Context, key string) (int64, error) {
	return c.client.LLen(ctx, key).Result()
}

// SAdd adds to set
func (c *RedisCache) SAdd(ctx context.Context, key string, members ...interface{}) error {
	args := make([]interface{}, len(members))
	for i, m := range members {
		data, _ := json.Marshal(m)
		args[i] = data
	}
	return c.client.SAdd(ctx, key, args...).Err()
}

// SMembers gets set members
func (c *RedisCache) SMembers(ctx context.Context, key string) ([]string, error) {
	return c.client.SMembers(ctx, key).Result()
}

// SIsMember checks if member exists
func (c *RedisCache) SIsMember(ctx context.Context, key string, member interface{}) (bool, error) {
	data, _ := json.Marshal(member)
	return c.client.SIsMember(ctx, key, data).Result()
}

// SRem removes from set
func (c *RedisCache) SRem(ctx context.Context, key string, members ...interface{}) error {
	args := make([]interface{}, len(members))
	for i, m := range members {
		data, _ := json.Marshal(m)
		args[i] = data
	}
	return c.client.SRem(ctx, key, args...).Err()
}

// ZAdd adds to sorted set
func (c *RedisCache) ZAdd(ctx context.Context, key string, score float64, member interface{}) error {
	data, _ := json.Marshal(member)
	return c.client.ZAdd(ctx, key, redis.Z{Score: score, Member: data}).Err()
}

// ZRange gets sorted set range by score
func (c *RedisCache) ZRange(ctx context.Context, key string, min, max string, offset, count int64) ([]string, error) {
	return c.client.ZRangeByScore(ctx, key, &redis.ZRangeBy{
		Min:    min,
		Max:    max,
		Offset: offset,
		Count:  count,
	}).Result()
}

// ZRemRangeByScore removes sorted set range by score
func (c *RedisCache) ZRemRangeByScore(ctx context.Context, key string, min, max string) error {
	return c.client.ZRemRangeByScore(ctx, key, min, max).Err()
}

// Ping tests connection
func (c *RedisCache) Ping(ctx context.Context) error {
	return c.client.Ping(ctx).Err()
}

// Close closes connection
func (c *RedisCache) Close() error {
	return c.client.Close()
}

// Lock acquires distributed lock
func (c *RedisCache) Lock(ctx context.Context, key string, expiration time.Duration) (bool, error) {
	return c.client.SetNX(ctx, key, "1", expiration).Result()
}

// Unlock releases distributed lock
func (c *RedisCache) Unlock(ctx context.Context, key string) error {
	return c.client.Del(ctx, key).Err()
}

// TryLock tries to acquire lock with retry
func (c *RedisCache) TryLock(ctx context.Context, key string, expiration time.Duration, retryInterval time.Duration, maxRetries int) (bool, error) {
	for i := 0; i < maxRetries; i++ {
		acquired, err := c.Lock(ctx, key, expiration)
		if err != nil {
			return false, err
		}
		if acquired {
			return true, nil
		}
		time.Sleep(retryInterval)
	}
	return false, nil
}
