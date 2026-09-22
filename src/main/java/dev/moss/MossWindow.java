package dev.moss;

import java.util.Objects;

/** An owning Java handle for a native {@code Moss_Window}. */
public final class MossWindow implements AutoCloseable {
    private long handle;
    private MossWindow(long handle) { this.handle = handle; }

    public static MossWindow create(String title, int width, int height) {
        Objects.requireNonNull(title, "title");
        MossNative.load();
        long handle = nCreate(title, width, height);
        if (handle == 0) throw new IllegalStateException("Moss_CreateWindow failed");
        return new MossWindow(handle);
    }
    public boolean shouldClose() { return nShouldClose(requireOpen()); }
    public void requestClose() { nRequestClose(requireOpen()); }
    public void setTitle(String title) { nSetTitle(requireOpen(), Objects.requireNonNull(title, "title")); }
    public int width() { requireOpen(); return nWidth(); }
    public int height() { requireOpen(); return nHeight(); }
    @Override public void close() { long current = handle; if (current != 0) { handle = 0; nDestroy(current); } }
    private long requireOpen() { if (handle == 0) throw new IllegalStateException("MossWindow is closed"); return handle; }

    private static native long nCreate(String title, int width, int height);
    private static native void nDestroy(long window);
    private static native boolean nShouldClose(long window);
    private static native void nRequestClose(long window);
    private static native void nSetTitle(long window, String title);
    private static native int nWidth();
    private static native int nHeight();
}