package org.davidjm.scripture;

import android.Manifest;
import android.app.Activity;
import android.app.AlarmManager;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.os.Build;
import android.provider.Settings;

import java.lang.reflect.Method;
import java.time.LocalDate;
import java.util.Calendar;
import java.util.Locale;

public final class DailyVerseScheduler {
    private static final String PREFS = "scripture-daily-notification";
    private static final String TIME = "time";
    private static final String REFERENCE = "reference";
    private static final String TEXT = "text";
    private static final String LAST_DAY = "last-day";
    private static final String PERMISSION_REQUESTED = "permission-requested";
    private static final String PERMISSION_PENDING = "permission-pending";
    private static final String PERMISSION_PENDING_AT = "permission-pending-at";
    private static final String CHANNEL = "daily-verse";
    private static final String STATE_UNAVAILABLE = "unavailable";
    private static final String STATE_PENDING = "pending";
    private static final String STATE_GRANTED = "granted";
    private static final String STATE_RUNTIME_DENIED = "runtime-denied";
    private static final String STATE_APP_BLOCKED = "app-blocked";
    private static final String STATE_CHANNEL_DISABLED = "channel-disabled";
    private static final int REQUEST_CODE = 4107;
    private static final int NOTIFICATION_ID = 4107;

    private DailyVerseScheduler() {
    }

    public static boolean isSupported() {
        try {
            Context context = context();
            return Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
                && context != null
                && context.getSystemService(NotificationManager.class) != null
                && context.getSystemService(AlarmManager.class) != null;
        } catch (Exception exception) {
            return false;
        }
    }

    public static synchronized boolean permissionRequested() {
        try {
            Context context = context();
            return context != null && preferences(context).getBoolean(PERMISSION_REQUESTED, false);
        } catch (Exception exception) {
            return false;
        }
    }

    public static synchronized String permissionState() {
        try {
            Context context = context();
            if (!isSupported() || context == null)
                return STATE_UNAVAILABLE;
            SharedPreferences preferences = preferences(context);
            if (preferences.getBoolean(PERMISSION_PENDING, false)) {
                long pendingAt = preferences.getLong(PERMISSION_PENDING_AT, 0);
                if (System.currentTimeMillis() - pendingAt < 10 * 60 * 1000L)
                    return STATE_PENDING;
                preferences.edit()
                    .putBoolean(PERMISSION_PENDING, false)
                    .remove(PERMISSION_PENDING_AT)
                    .commit();
            }
            if (Build.VERSION.SDK_INT >= 33
                && context.checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS)
                    != PackageManager.PERMISSION_GRANTED)
                return STATE_RUNTIME_DENIED;
            NotificationManager manager = context.getSystemService(NotificationManager.class);
            if (manager == null || !manager.areNotificationsEnabled())
                return STATE_APP_BLOCKED;
            NotificationChannel channel = manager.getNotificationChannel(CHANNEL);
            if (channel != null && channel.getImportance() == NotificationManager.IMPORTANCE_NONE)
                return STATE_CHANNEL_DISABLED;
            return STATE_GRANTED;
        } catch (Exception exception) {
            return STATE_UNAVAILABLE;
        }
    }

    public static synchronized String consumePermissionResult() {
        try {
            Context context = context();
            if (context == null)
                return STATE_UNAVAILABLE;
            String state = permissionState();
            if (STATE_PENDING.equals(state))
                return state;
            preferences(context).edit()
                .putBoolean(PERMISSION_PENDING, false)
                .remove(PERMISSION_PENDING_AT)
                .commit();
            return state;
        } catch (Exception exception) {
            return STATE_UNAVAILABLE;
        }
    }

    public static synchronized void onPermissionResult(int requestCode, int[] grantResults) {
        if (requestCode != REQUEST_CODE)
            return;
        try {
            Context context = context();
            if (context != null)
                preferences(context).edit()
                    .putBoolean(PERMISSION_PENDING, false)
                    .remove(PERMISSION_PENDING_AT)
                    .commit();
        } catch (Exception exception) {
        }
    }

    public static synchronized String requestPermission(Activity activity) {
        try {
            if (!isSupported())
                return STATE_UNAVAILABLE;
            String state = permissionState();
            if (!STATE_RUNTIME_DENIED.equals(state))
                return state;
            Context context = context();
            if (context == null)
                return STATE_UNAVAILABLE;
            SharedPreferences preferences = preferences(context);
            if (preferences.getBoolean(PERMISSION_REQUESTED, false))
                return state;
            if (activity == null)
                return STATE_UNAVAILABLE;
            if (!preferences.edit()
                .putBoolean(PERMISSION_REQUESTED, true)
                .putBoolean(PERMISSION_PENDING, true)
                .putLong(PERMISSION_PENDING_AT, System.currentTimeMillis())
                .commit())
                return STATE_UNAVAILABLE;
            if (Build.VERSION.SDK_INT < 33)
                return consumePermissionResult();
            activity.requestPermissions(
                new String[]{Manifest.permission.POST_NOTIFICATIONS}, REQUEST_CODE);
            return STATE_PENDING;
        } catch (Exception exception) {
            return STATE_UNAVAILABLE;
        }
    }

    public static synchronized boolean openNotificationSettings(String state) {
        try {
            if (!isSupported())
                return false;
            Context context = context();
            if (context == null)
                return false;
            Intent settingsIntent;
            if (STATE_APP_BLOCKED.equals(state) || STATE_RUNTIME_DENIED.equals(state)) {
                settingsIntent = new Intent(Settings.ACTION_APP_NOTIFICATION_SETTINGS)
                    .putExtra(Settings.EXTRA_APP_PACKAGE, context.getPackageName());
            } else if (STATE_CHANNEL_DISABLED.equals(state)) {
                settingsIntent = new Intent(Settings.ACTION_CHANNEL_NOTIFICATION_SETTINGS)
                    .putExtra(Settings.EXTRA_APP_PACKAGE, context.getPackageName())
                    .putExtra(Settings.EXTRA_CHANNEL_ID, CHANNEL);
            } else {
                return false;
            }
            settingsIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            context.startActivity(settingsIntent);
            return true;
        } catch (Exception exception) {
            return false;
        }
    }

    public static boolean schedule(String value) {
        try {
            if (!validTime(value) || !isSupported() || !STATE_GRANTED.equals(permissionState()))
                return false;
            Context context = context();
            SharedPreferences preferences = preferences(context);
            boolean hadTime = preferences.contains(TIME);
            String previous = preferences.getString(TIME, "");
            if (!preferences.edit().putString(TIME, value).commit())
                return false;
            if (!scheduleNext(context)) {
                SharedPreferences.Editor rollback = preferences.edit();
                if (hadTime)
                    rollback.putString(TIME, previous);
                else
                    rollback.remove(TIME);
                rollback.commit();
                return false;
            }
            return true;
        } catch (Exception exception) {
            return false;
        }
    }

    public static boolean cancel() {
        try {
            Context context = context();
            if (context == null)
                return false;
            AlarmManager manager = context.getSystemService(AlarmManager.class);
            if (manager != null)
                manager.cancel(alarmIntent(context));
            return preferences(context).edit().remove(TIME).commit();
        } catch (Exception exception) {
            return false;
        }
    }

    public static boolean updateSnapshot(String reference, String text) {
        try {
            Context context = context();
            if (context == null)
                return false;
            String safeReference = reference == null ? "" : reference.trim();
            String safeText = compactNotificationText(text);
            if (safeReference.length() > 120)
                safeReference = safeReference.substring(0, 120);
            if (safeText.length() > 32768)
                safeText = safeText.substring(0, 32768);
            return preferences(context).edit()
                .putString(REFERENCE, safeReference)
                .putString(TEXT, safeText)
                .commit();
        } catch (Exception exception) {
            return false;
        }
    }

    static synchronized boolean postIfNeeded(Context context) {
        try {
            if (context == null || !isSupported() || !STATE_GRANTED.equals(permissionState()))
                return false;
            SharedPreferences preferences = preferences(context);
            String day = LocalDate.now().toString();
            if (day.equals(preferences.getString(LAST_DAY, "")))
                return false;
            NotificationManager manager = context.getSystemService(NotificationManager.class);
            if (manager == null)
                return false;
            createChannel(manager);
            NotificationChannel channel = manager.getNotificationChannel(CHANNEL);
            if (channel != null && channel.getImportance() == NotificationManager.IMPORTANCE_NONE)
                return false;
            String reference = preferences.getString(REFERENCE, "").trim();
            String text = compactNotificationText(preferences.getString(TEXT, ""));
            if (!text.equals(preferences.getString(TEXT, "")))
                preferences.edit().putString(TEXT, text).commit();
            if (text.isEmpty())
                text = "Open Scripture for today's verse.";
            Intent launch = context.getPackageManager().getLaunchIntentForPackage(context.getPackageName());
            PendingIntent contentIntent = null;
            if (launch != null) {
                launch.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TOP);
                contentIntent = PendingIntent.getActivity(
                    context, REQUEST_CODE, launch,
                    PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
            }
            int icon = context.getResources().getIdentifier(
                "ic_notification", "drawable", context.getPackageName());
            if (icon == 0)
                icon = android.R.drawable.ic_dialog_info;
            Notification.Builder builder = new Notification.Builder(context, CHANNEL)
                .setSmallIcon(icon)
                .setContentTitle(reference.isEmpty() ? "Scripture" : reference)
                .setContentText(text)
                .setCategory(Notification.CATEGORY_REMINDER)
                .setAutoCancel(true);
            if (contentIntent != null)
                builder.setContentIntent(contentIntent);
            manager.notify(NOTIFICATION_ID, builder.build());
            return preferences.edit().putString(LAST_DAY, day).commit();
        } catch (Exception exception) {
            return false;
        }
    }

    static synchronized boolean scheduleNext(Context context) {
        try {
            if (context == null || !isSupported() || !STATE_GRANTED.equals(permissionState()))
                return false;
            String value = preferences(context).getString(TIME, "");
            if (!validTime(value))
                return false;
            String[] parts = value.split(":", -1);
            int hour = Integer.parseInt(parts[0]);
            int minute = Integer.parseInt(parts[1]);
            Calendar next = Calendar.getInstance();
            next.set(Calendar.HOUR_OF_DAY, hour);
            next.set(Calendar.MINUTE, minute);
            next.set(Calendar.SECOND, 0);
            next.set(Calendar.MILLISECOND, 0);
            if (next.getTimeInMillis() <= System.currentTimeMillis())
                next.add(Calendar.DAY_OF_MONTH, 1);
            AlarmManager manager = context.getSystemService(AlarmManager.class);
            if (manager == null)
                return false;
            manager.setAndAllowWhileIdle(
                AlarmManager.RTC_WAKEUP, next.getTimeInMillis(), alarmIntent(context));
            return true;
        } catch (Exception exception) {
            return false;
        }
    }

    static String compactNotificationText(String value) {
        if (value == null)
            return "";
        String text = value.replace("\r\n", "\n").trim();
        String lower = text.toLowerCase(Locale.ROOT);
        int cut = text.length();
        String[] markers = {
            "scripture quotations are from the esv",
            "used by permission",
            "all rights reserved",
            "crossway",
            "esv.org",
            "\u00a9"
        };
        for (String marker : markers) {
            int index = lower.indexOf(marker);
            if (index >= 0 && index < cut)
                cut = index;
        }
        if (cut < text.length())
            text = text.substring(0, cut).trim();
        return text.length() > 32768 ? text.substring(0, 32768) : text;
    }

    private static void createChannel(NotificationManager manager) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O)
            return;
        NotificationChannel channel = new NotificationChannel(
            CHANNEL, "Daily verse", NotificationManager.IMPORTANCE_DEFAULT);
        manager.createNotificationChannel(channel);
    }

    private static PendingIntent alarmIntent(Context context) {
        Intent intent = new Intent(context, DailyVerseReceiver.class)
            .setAction(DailyVerseReceiver.ACTION_DAILY)
            .setPackage(context.getPackageName());
        return PendingIntent.getBroadcast(
            context, REQUEST_CODE, intent,
            PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
    }

    private static boolean validTime(String value) {
        return value != null && value.matches("(?:[01]\\d|2[0-3]):[0-5]\\d");
    }

    private static SharedPreferences preferences(Context context) {
        return context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
    }

    private static Context context() throws Exception {
        Class<?> activityThread = Class.forName("android.app.ActivityThread");
        Method currentApplication = activityThread.getMethod("currentApplication");
        Object application = currentApplication.invoke(null);
        if (!(application instanceof Context))
            throw new IllegalStateException("application context unavailable");
        return (Context) application;
    }
}
