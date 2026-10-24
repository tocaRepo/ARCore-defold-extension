package com.defold.arcore;

import android.Manifest;
import android.app.Activity;
import android.app.Fragment;
import android.content.pm.PackageManager;
import android.os.Bundle;
import java.lang.ref.WeakReference;

/** Permission and display plumbing only. ARCore exclusively owns the camera. */
public final class CameraPermission extends Fragment {
    private static final String TAG = "defold-arcore-camera-permission";
    private static volatile int state = 0; // pending, granted, denied
    private static boolean requested;
    private static WeakReference<Activity> owner = new WeakReference<Activity>(null);
    private boolean asked;

    public static synchronized void retry() {
        if (state < 0) { state = 0; requested = false; }
    }

    public static synchronized int ensure(final Activity activity) {
        if (activity.checkSelfPermission(Manifest.permission.CAMERA) == PackageManager.PERMISSION_GRANTED) {
            state = 1;
            return state;
        }
        if (state == 1) { state = -1; requested = true; } // Revoked in Settings.
        if (owner.get() != activity) {
            owner = new WeakReference<Activity>(activity);
            if (state == 0) requested = false; // Reconcile restored fragments on the UI thread.
        }
        if (!requested && state >= 0) {
            requested = true;
            activity.runOnUiThread(new Runnable() {
                @Override public void run() {
                    if (activity.isFinishing() || activity.isDestroyed()) {
                        synchronized (CameraPermission.class) { requested = false; }
                        return;
                    }
                    if (activity.getFragmentManager().findFragmentByTag(TAG) == null) {
                        activity.getFragmentManager().beginTransaction()
                            .add(new CameraPermission(), TAG).commitAllowingStateLoss();
                    }
                }
            });
        }
        return state;
    }

    @Override public void onCreate(Bundle saved) {
        super.onCreate(saved);
        asked = saved != null && saved.getBoolean("asked", false);
        if (!asked) {
            asked = true;
            requestPermissions(new String[] { Manifest.permission.CAMERA }, 7301);
        }
    }
    @Override public void onSaveInstanceState(Bundle out) {
        out.putBoolean("asked", asked);
        super.onSaveInstanceState(out);
    }
    @Override public void onRequestPermissionsResult(int code, String[] permissions, int[] results) {
        super.onRequestPermissionsResult(code, permissions, results);
        if (code != 7301) return;
        synchronized (CameraPermission.class) {
            state = results.length > 0 && results[0] == PackageManager.PERMISSION_GRANTED ? 1 : -1;
        }
        if (getFragmentManager() != null) {
            getFragmentManager().beginTransaction().remove(this).commitAllowingStateLoss();
        }
    }
    public static int rotation(Activity activity) {
        return activity.getWindowManager().getDefaultDisplay().getRotation();
    }
}
