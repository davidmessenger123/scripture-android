package org.davidjm.scripture;

import org.qtproject.qt.android.bindings.QtActivity;

public final class MainActivity extends QtActivity {
    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        DailyVerseScheduler.onPermissionResult(requestCode, grantResults);
    }
}
