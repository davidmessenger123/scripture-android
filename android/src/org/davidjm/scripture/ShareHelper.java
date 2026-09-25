package org.davidjm.scripture;

import android.content.ClipData;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;

import java.io.File;
import java.lang.reflect.Method;

public final class ShareHelper {
    private ShareHelper() {
    }

    public static boolean shareText(String text) {
        try {
            if (text == null || text.trim().isEmpty() || text.length() > 65536)
                return false;
            Context context = current();
            if (context == null)
                return false;
            Intent send = new Intent(Intent.ACTION_SEND)
                .setType("text/plain")
                .putExtra(Intent.EXTRA_TEXT, text);
            return startChooser(context, send, null);
        } catch (Exception exception) {
            return false;
        }
    }

    public static boolean shareImage(String path, String text) {
        try {
            if (path == null || path.length() > 4096)
                return false;
            Context context = current();
            if (context == null)
                return false;
            File file = new File(path).getCanonicalFile();
            if (!VerseShareProvider.isAllowed(context, file))
                return false;
            Uri uri = new Uri.Builder()
                .scheme("content")
                .authority(context.getPackageName() + VerseShareProvider.AUTHORITY_SUFFIX)
                .appendPath("cards")
                .appendPath(file.getName())
                .build();
            String safeText = text == null ? "" : text.trim();
            if (safeText.length() > 65536)
                safeText = safeText.substring(0, 65536);
            Intent send = new Intent(Intent.ACTION_SEND)
                .setType("image/png")
                .putExtra(Intent.EXTRA_STREAM, uri)
                .putExtra(Intent.EXTRA_TEXT, safeText)
                .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            send.setClipData(ClipData.newRawUri("verse-card", uri));
            return startChooser(context, send, uri);
        } catch (Exception exception) {
            return false;
        }
    }

    private static Context current() throws Exception {
        Class<?> activityThread = Class.forName("android.app.ActivityThread");
        Method currentApplication = activityThread.getMethod("currentApplication");
        Object application = currentApplication.invoke(null);
        return application instanceof Context ? (Context) application : null;
    }

    private static boolean startChooser(Context context, Intent send, Uri uri) {
        try {
            Intent chooser = Intent.createChooser(send, null);
            chooser.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            if (uri != null)
                chooser.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            context.startActivity(chooser);
            return true;
        } catch (Exception exception) {
            return false;
        }
    }
}
