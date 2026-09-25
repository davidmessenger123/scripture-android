package org.davidjm.scripture;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

public final class DailyVerseReceiver extends BroadcastReceiver {
    public static final String ACTION_DAILY = "org.davidjm.scripture.DAILY_VERSE";
    public static final String ACTION_BOOT = "android.intent.action.BOOT_COMPLETED";
    public static final String ACTION_TIME_SET = "android.intent.action.TIME_SET";
    public static final String ACTION_TIMEZONE_CHANGED = "android.intent.action.TIMEZONE_CHANGED";
    public static final String ACTION_PACKAGE_REPLACED = "android.intent.action.MY_PACKAGE_REPLACED";

    @Override
    public void onReceive(Context context, Intent intent) {
        String action = intent == null ? "" : intent.getAction();
        if (ACTION_DAILY.equals(action)) {
            DailyVerseScheduler.postIfNeeded(context);
            DailyVerseScheduler.scheduleNext(context);
            return;
        }
        if (ACTION_BOOT.equals(action) || ACTION_TIME_SET.equals(action)
                || ACTION_TIMEZONE_CHANGED.equals(action) || ACTION_PACKAGE_REPLACED.equals(action)) {
            DailyVerseScheduler.scheduleNext(context);
        }
    }
}
