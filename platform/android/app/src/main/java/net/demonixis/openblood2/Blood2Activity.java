package net.demonixis.openblood2;

import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.provider.Settings;
import android.util.Log;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import javax.net.ssl.HttpsURLConnection;
import org.libsdl.app.SDLActivity;

/** Starts the game; SDL is linked statically into libmain.so. */
public class Blood2Activity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] {"main"};
    }

    /**
     * Game data lives in the shared openblood2/ folders, which Android 11+ reads only with
     * "all files access". The setup screen shows whether it is granted and opens the system page.
     */
    public static boolean hasAllFilesAccess() {
        return Build.VERSION.SDK_INT < Build.VERSION_CODES.R || Environment.isExternalStorageManager();
    }

    public static void requestAllFilesAccess() {
        SDLActivity activity = mSingleton;
        if (activity == null || hasAllFilesAccess()) {
            return;
        }
        try {
            activity.startActivity(new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                    Uri.parse("package:" + activity.getPackageName())));
        } catch (Exception e) {
            Log.w("OpenBlood2", "All files access request failed", e);
        }
    }

    /**
     * One HTTPS GET into {@code path} on the calling (native) thread. Redirects are returned, not
     * followed. Returns "status\nlocation\nbytes" or "!error"; downloadProgress cancels.
     */
    public static String httpsGet(String address, String path, long limit, long handle) {
        HttpURLConnection connection = null;
        try {
            URL url = new URL(address);
            connection = (HttpURLConnection) url.openConnection();
            if (!(connection instanceof HttpsURLConnection)) {
                return "!HTTPS required";
            }
            connection.setInstanceFollowRedirects(false);
            connection.setConnectTimeout(15000);
            connection.setReadTimeout(30000);
            connection.setRequestProperty("User-Agent", "OpenBlood2/1");
            int status = connection.getResponseCode();
            String location = "";
            if (status >= 300 && status < 400 && connection.getHeaderField("Location") != null) {
                location = new URL(url, connection.getHeaderField("Location")).toString();
            }
            long total = 0;
            try (FileOutputStream out = new FileOutputStream(path)) {
                if (status == 200) {
                    try (InputStream in = connection.getInputStream()) {
                        byte[] buffer = new byte[65536];
                        for (int n; (n = in.read(buffer)) > 0;) {
                            if (n > limit - total) {
                                return "!Download exceeds the expected size";
                            }
                            out.write(buffer, 0, n);
                            total += n;
                            if (!downloadProgress(handle, total)) {
                                return "!Installation cancelled";
                            }
                        }
                    }
                }
            }
            return status + "\n" + location + "\n" + total;
        } catch (Exception e) {
            return "!" + e;
        } finally {
            if (connection != null) {
                connection.disconnect();
            }
        }
    }

    private static native boolean downloadProgress(long handle, long bytes);
}
