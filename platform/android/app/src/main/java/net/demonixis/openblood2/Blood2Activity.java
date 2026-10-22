package net.demonixis.openblood2;

import android.Manifest;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;
import android.util.Log;
import java.util.Arrays;
import org.libsdl.app.SDLActivity;

/** Starts the game; SDL is linked statically into libmain.so. */
public class Blood2Activity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] {"main"};
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        requestAllFilesAccess();
    }

    /**
     * Builds that declare MANAGE_EXTERNAL_STORAGE (the Quest flavor) read the shared
     * openblood2/ data folders; Android 11+ grants that from the system "all files access" page.
     */
    private void requestAllFilesAccess() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R || Environment.isExternalStorageManager()) {
            return;
        }
        try {
            String[] requested = getPackageManager()
                    .getPackageInfo(getPackageName(), PackageManager.GET_PERMISSIONS).requestedPermissions;
            if (requested == null
                    || !Arrays.asList(requested).contains(Manifest.permission.MANAGE_EXTERNAL_STORAGE)) {
                return;
            }
            startActivity(new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                    Uri.parse("package:" + getPackageName())));
        } catch (Exception e) {
            Log.w("OpenBlood2", "All files access request failed", e);
        }
    }
}
