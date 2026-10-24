package com.defold.arcore;
import android.app.Activity;
public final class CameraPermission {
    public static int permission = 1;
    public static int displayRotation = 0;
    public static int ensure(Activity activity) { return permission; }
    public static void retry() {}
    public static int rotation(Activity activity) { return displayRotation; }
}
