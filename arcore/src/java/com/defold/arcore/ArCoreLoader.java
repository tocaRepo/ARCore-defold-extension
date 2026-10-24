package com.defold.arcore;

import dalvik.system.BaseDexClassLoader;

/** Load through the app class loader, including APK!/lib paths and JNI_OnLoad. */
public final class ArCoreLoader {
    private ArCoreLoader() {}
    public static String load() {
        ClassLoader loader = ArCoreLoader.class.getClassLoader();
        if (!(loader instanceof BaseDexClassLoader)) {
            throw new UnsatisfiedLinkError("ARCore requires the application's Android class loader");
        }
        String path = ((BaseDexClassLoader) loader).findLibrary("arcore_sdk_c");
        if (path == null) throw new UnsatisfiedLinkError("ARCore C library is absent for this ABI");
        System.loadLibrary("arcore_sdk_c");
        System.loadLibrary("arcore_sdk_jni");
        return path;
    }
}
