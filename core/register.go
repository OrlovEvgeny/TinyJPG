package core

import "context"

type KeyType int

const (
	ImageMagic KeyType = iota
	Log
)

//Register
func Register(ctx context.Context, key KeyType, value interface{}) context.Context {
	return context.WithValue(ctx, key, value)
}
