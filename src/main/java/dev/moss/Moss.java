package dev.moss;

/** Entry points that do not belong to a specific Moss resource. */
public final class Moss {
    private Moss() { }
    private static native void nPollEvents();
    private static native boolean nSetClipboardText(String text);
    private static native String nGetClipboardText();
    private static native int nAvailableCpuCores();
    private static native int nCpuCacheLineSize();
    private static native int nSystemRamMiB();

    public static void pollEvents() { MossNative.load(); nPollEvents(); }
    public static boolean setClipboardText(String text) { MossNative.load(); return nSetClipboardText(text); }
    public static String clipboardText() { MossNative.load(); return nGetClipboardText(); }
    public static int availableCpuCores() { MossNative.load(); return nAvailableCpuCores(); }
    public static int cpuCacheLineSize() { MossNative.load(); return nCpuCacheLineSize(); }
    public static int systemRamMiB() { MossNative.load(); return nSystemRamMiB(); }
}