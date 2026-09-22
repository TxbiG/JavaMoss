package dev.moss;

/** Immutable two-component float value for future Moss math-facing APIs. */
public record Vec2(float x, float y) {
    public static final Vec2 ZERO = new Vec2(0f, 0f);
    public Vec2 add(Vec2 other) { return new Vec2(x + other.x, y + other.y); }
    public Vec2 scale(float scalar) { return new Vec2(x * scalar, y * scalar); }
    public float lengthSquared() { return x * x + y * y; }
}