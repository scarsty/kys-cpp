package com.kysgame.kyschess;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.util.TypedValue;
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
import java.io.ByteArrayOutputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;

public class KysActivity extends SDLActivity {

    private static final String TAG = "KysActivity";
    private static final String PREFS_NAME = "kys_prefs";
    private static final String PREF_ASSET_MARKER = "asset_marker";

    public static native void nativeInjectRightClick();

    private int dp(int value) {
        return Math.round(TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_DIP,
                value,
                getResources().getDisplayMetrics()));
    }

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
        SharedPreferences prefs = getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
        File destDir = new File(getFilesDir(), "game");
        String packagedVersion = readPackagedReleaseVersion();
        String desiredMarker = buildAssetExtractionMarker(packagedVersion);
        String installedMarker = prefs.getString(PREF_ASSET_MARKER, "");

        if (desiredMarker.equals(installedMarker) && destDir.isDirectory()) {
            Log.i(TAG, "Assets already extracted for marker " + desiredMarker + ", skipping.");
            return;
        }

        Log.i(TAG, "Refreshing game assets for marker " + desiredMarker + "...");
        try {
            prepareGameDirForExtraction(destDir);
            copyAssetDir(getAssets(), "game", destDir, true);
            copyPackagedSaveAssets(getAssets(), destDir);
            prefs.edit().putString(PREF_ASSET_MARKER, desiredMarker).apply();
            Log.i(TAG, "Asset extraction complete.");
        } catch (IOException e) {
            Log.e(TAG, "Asset extraction failed", e);
        }
    }

    private String buildAssetExtractionMarker(String packagedVersion) {
        try {
            PackageInfo packageInfo = getPackageManager().getPackageInfo(getPackageName(), 0);
            long versionCode = Build.VERSION.SDK_INT >= Build.VERSION_CODES.P
                    ? packageInfo.getLongVersionCode()
                    : packageInfo.versionCode;
            String versionName = packageInfo.versionName != null ? packageInfo.versionName : "";
            return packagedVersion + "|" + versionName + "|" + versionCode + "|" + packageInfo.lastUpdateTime;
        } catch (PackageManager.NameNotFoundException e) {
            Log.w(TAG, "Unable to resolve package info for asset marker", e);
            return packagedVersion;
        }
    }

    private String readPackagedReleaseVersion() {
        try {
            String releaseIni = readTextAsset("game/config/release.ini");
            for (String rawLine : releaseIni.split("\\r?\\n")) {
                String line = rawLine.trim();
                if (line.startsWith("version")) {
                    int equalsIndex = line.indexOf('=');
                    if (equalsIndex >= 0 && equalsIndex + 1 < line.length()) {
                        return line.substring(equalsIndex + 1).trim();
                    }
                }
            }
        } catch (IOException e) {
            Log.w(TAG, "Unable to read packaged release version", e);
        }
        return "dev";
    }

    private String readTextAsset(String assetPath) throws IOException {
        try (InputStream in = getAssets().open(assetPath);
             ByteArrayOutputStream buffer = new ByteArrayOutputStream()) {
            byte[] chunk = new byte[4096];
            int read;
            while ((read = in.read(chunk)) != -1) {
                buffer.write(chunk, 0, read);
            }
            return buffer.toString(StandardCharsets.UTF_8.name());
        }
    }

    private void prepareGameDirForExtraction(File destDir) throws IOException {
        if (!destDir.exists() && !destDir.mkdirs()) {
            throw new IOException("Failed to create directory: " + destDir);
        }

        File[] children = destDir.listFiles();
        if (children == null) {
            return;
        }

        for (File child : children) {
            if ("save".equals(child.getName())) {
                continue;
            }
            deleteRecursively(child);
        }
    }

    private void deleteRecursively(File file) throws IOException {
        if (file.isDirectory()) {
            File[] children = file.listFiles();
            if (children != null) {
                for (File child : children) {
                    deleteRecursively(child);
                }
            }
        }

        if (file.exists() && !file.delete()) {
            throw new IOException("Failed to delete " + file.getAbsolutePath());
        }
    }

    private void copyAssetDir(AssetManager am, String assetPath, File destDir, boolean skipPackagedSaveDir) throws IOException {
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
            if (skipPackagedSaveDir && "game".equals(assetPath) && "save".equals(child)) {
                continue;
            }
            String childAsset = assetPath + "/" + child;
            File childDest = new File(destDir, child);
            copyAssetDir(am, childAsset, childDest, skipPackagedSaveDir);
        }
    }

    private void copyPackagedSaveAssets(AssetManager am, File gameDir) throws IOException {
        File saveDir = new File(gameDir, "save");
        if (!saveDir.exists() && !saveDir.mkdirs()) {
            throw new IOException("Failed to create directory: " + saveDir);
        }

        copyAssetFile(am, "game/save/game.db", new File(saveDir, "game.db"));

        String[] saveEntries = am.list("game/save");
        if (saveEntries == null) {
            return;
        }

        for (String entry : saveEntries) {
            if (entry.endsWith(".grp.zip")) {
                copyAssetFile(am, "game/save/" + entry, new File(saveDir, entry));
            }
        }

        copyAssetFileIfMissing(am, "game/save/setting.json", new File(saveDir, "setting.json"));
    }

    private void copyAssetFileIfMissing(AssetManager am, String assetPath, File destFile) throws IOException {
        if (destFile.exists()) {
            return;
        }
        copyAssetFile(am, assetPath, destFile);
    }

    private void copyAssetFile(AssetManager am, String assetPath, File destFile) throws IOException {
        File parent = destFile.getParentFile();
        if (parent != null && !parent.exists() && !parent.mkdirs()) {
            throw new IOException("Failed to create directory: " + parent);
        }

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
            btn.setTextSize(32);
            btn.setTextColor(0xCCFFFFFF);
            btn.setBackgroundColor(0x44000000);
            btn.setPadding(32, 16, 32, 16);

            FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.WRAP_CONTENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT,
                    Gravity.BOTTOM | Gravity.END
            );
            params.setMargins(0, 0, dp(16), dp(16));

            btn.setOnTouchListener((v, event) -> {
                switch (event.getAction()) {
                    case MotionEvent.ACTION_DOWN:
                        return true;
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
