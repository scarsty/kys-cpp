package com.kysgame.kyschess;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.AssetManager;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

public class KysActivity extends SDLActivity {

    private static final String TAG = "KysActivity";
    private static final int ASSET_VERSION = 2;

    public static native void nativeInjectRightClick();

    @Override
    protected String[] getLibraries() {
        // SDL3 is statically linked into libmain.so — do not load a separate libSDL3.so
        return new String[]{"main"};
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        extractAssetsIfNeeded();
        super.onCreate(savedInstanceState);
        enterImmersiveFullscreen();
        addRightClickOverlay();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            enterImmersiveFullscreen();
        }
    }

    @SuppressWarnings("deprecation")
    private void enterImmersiveFullscreen() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            getWindow().setDecorFitsSystemWindows(false);
            WindowInsetsController ctrl = getWindow().getInsetsController();
            if (ctrl != null) {
                ctrl.hide(WindowInsets.Type.statusBars() | WindowInsets.Type.navigationBars());
                ctrl.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                | View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
        }
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    }

    private void extractAssetsIfNeeded() {
        SharedPreferences prefs = getSharedPreferences("kys_prefs", Context.MODE_PRIVATE);
        int installed = prefs.getInt("asset_version", 0);
        if (installed >= ASSET_VERSION) {
            Log.i(TAG, "Assets already extracted (v" + installed + "), skipping.");
            return;
        }

        Log.i(TAG, "Extracting game assets (v" + ASSET_VERSION + ")...");
        File destDir = new File(getFilesDir(), "game");
        try {
            copyAssetDir(getAssets(), "game", destDir);
            prefs.edit().putInt("asset_version", ASSET_VERSION).apply();
            Log.i(TAG, "Asset extraction complete.");
        } catch (IOException e) {
            Log.e(TAG, "Asset extraction failed", e);
        }
    }

    private void copyAssetDir(AssetManager am, String assetPath, File destDir) throws IOException {
        String[] children = am.list(assetPath);
        if (children == null || children.length == 0) {
            // It's a file — copy it
            copyAssetFile(am, assetPath, destDir);
            return;
        }

        // It's a directory — recurse
        if (!destDir.exists() && !destDir.mkdirs()) {
            throw new IOException("Failed to create directory: " + destDir);
        }
        for (String child : children) {
            String childAsset = assetPath + "/" + child;
            File childDest = new File(destDir, child);
            copyAssetDir(am, childAsset, childDest);
        }
    }

    private void copyAssetFile(AssetManager am, String assetPath, File destFile) throws IOException {
        destFile.getParentFile().mkdirs();

        try (InputStream in = am.open(assetPath);
             FileOutputStream out = new FileOutputStream(destFile)) {
            byte[] buf = new byte[8192];
            int len;
            while ((len = in.read(buf)) > 0) {
                out.write(buf, 0, len);
            }
        }
    }

    private void addRightClickOverlay() {
        runOnUiThread(() -> {
            // Find the SDL content view
            ViewGroup contentView = findViewById(android.R.id.content);
            if (contentView == null) return;

            // Create the right-click button
            TextView btn = new TextView(this);
            btn.setText("✕");
            btn.setTextSize(28);
            btn.setTextColor(0xCCFFFFFF);
            btn.setBackgroundColor(0x44000000);
            btn.setPadding(32, 16, 32, 16);

            FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.WRAP_CONTENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT,
                    Gravity.BOTTOM | Gravity.END
            );
            params.setMargins(0, 0, 24, 24);

            btn.setOnTouchListener((v, event) -> {
                switch (event.getAction()) {
                    case MotionEvent.ACTION_DOWN:
                    case MotionEvent.ACTION_UP:
                        nativeInjectRightClick();
                        return true;
                }
                return false;
            });

            // Wrap in FrameLayout overlay
            FrameLayout overlay = new FrameLayout(this);
            overlay.addView(btn, params);
            contentView.addView(overlay, new ViewGroup.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.MATCH_PARENT
            ));
        });
    }
}
