package org.davidjm.scripture;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;

public final class VerseShareProvider extends ContentProvider {
    public static final String AUTHORITY_SUFFIX = ".share";

    @Override
    public boolean onCreate() {
        return true;
    }

    @Override
    public String getType(Uri uri) {
        return allowedFile(uri) == null ? null : "image/png";
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection,
                        String[] selectionArgs, String sortOrder) {
        File file = allowedFile(uri);
        if (file == null)
            return null;
        String[] columns = projection == null
            ? new String[]{OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE}
            : projection;
        MatrixCursor cursor = new MatrixCursor(columns, 1);
        MatrixCursor.RowBuilder row = cursor.newRow();
        for (String column : columns) {
            if (OpenableColumns.DISPLAY_NAME.equals(column))
                row.add(file.getName());
            else if (OpenableColumns.SIZE.equals(column))
                row.add(file.length());
            else
                row.add(null);
        }
        cursor.setNotificationUri(getContext().getContentResolver(), uri);
        return cursor;
    }

    @Override
    public ParcelFileDescriptor openFile(Uri uri, String mode) throws FileNotFoundException {
        File file = allowedFile(uri);
        if (file == null || !"r".equals(mode))
            throw new FileNotFoundException();
        return ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_ONLY);
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        return 0;
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        return null;
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        return 0;
    }

    public static boolean isAllowed(Context context, File candidate) {
        if (context == null || candidate == null || !candidate.isFile())
            return false;
        try {
            File file = candidate.getCanonicalFile();
            if (!file.getName().matches("verse-card-[0-9a-f]{24}\\.png")
                || file.length() > 8 * 1024 * 1024)
                return false;
            File files = context.getFilesDir().getCanonicalFile();
            File cache = context.getCacheDir().getCanonicalFile();
            String path = file.getPath();
            return path.startsWith(files.getPath() + File.separator)
                || path.startsWith(cache.getPath() + File.separator);
        } catch (IOException exception) {
            return false;
        }
    }

    private File findFile(File directory, String name, int depth) {
        if (directory == null || depth > 4)
            return null;
        File direct = new File(directory, name);
        if (isAllowed(getContext(), direct))
            return direct;
        File[] children = directory.listFiles();
        if (children == null)
            return null;
        for (File child : children) {
            if (!child.isDirectory())
                continue;
            File found = findFile(child, name, depth + 1);
            if (found != null)
                return found;
        }
        return null;
    }

    private File allowedFile(Uri uri) {
        try {
            if (getContext() == null || uri == null)
                return null;
            if (!getContext().getPackageName().concat(AUTHORITY_SUFFIX).equals(uri.getAuthority()))
                return null;
            java.util.List<String> segments = uri.getPathSegments();
            if (segments.size() != 2 || !"cards".equals(segments.get(0)))
                return null;
            String name = segments.get(1);
            if (name.length() > 64 || !name.matches("verse-card-[0-9a-f]{24}\\.png"))
                return null;
            File file = findFile(getContext().getFilesDir(), name, 0);
            if (file != null)
                return file;
            return findFile(getContext().getCacheDir(), name, 0);
        } catch (Exception exception) {
            return null;
        }
    }
}
