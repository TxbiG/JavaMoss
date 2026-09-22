package dev.moss;

/** Internal native-library bootstrap. */
final class MossNative {
    static {
        String explicitPath = System.getProperty("moss.native.path");
        if (explicitPath == null || explicitPath.isBlank()) System.loadLibrary("moss_jni");
        else System.load(explicitPath);
    }
    static void load() { }
    private MossNative() { }
}