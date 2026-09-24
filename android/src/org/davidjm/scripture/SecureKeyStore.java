package org.davidjm.scripture;

import android.content.Context;
import android.content.SharedPreferences;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import android.util.Base64;

import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.security.KeyStore;
import java.util.Cipher;

import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import javax.crypto.spec.GCMParameterSpec;

public final class SecureKeyStore {
    private static final String ALIAS = "scripture-esv-api-key";
    private static final String PREFS = "scripture-secure-settings";
    private static final String CIPHERTEXT = "esv-ciphertext";
    private static final String IV = "esv-iv";
    private static final int MAX_KEY_BYTES = 512;
    private static final int MAX_CIPHERTEXT_CHARS = 1024;
    private static final int MAX_IV_CHARS = 64;

    private SecureKeyStore() {
    }

    public static synchronized boolean hasStoredValue() {
        try {
            SharedPreferences preferences = preferences();
            return preferences.getString(CIPHERTEXT, "").length() > 0
                || preferences.getString(IV, "").length() > 0;
        } catch (Exception exception) {
            return true;
        }
    }

    public static synchronized String get() {
        try {
            SharedPreferences preferences = preferences();
            String encoded = preferences.getString(CIPHERTEXT, "");
            String encodedIv = preferences.getString(IV, "");
            if (encoded.isEmpty() || encodedIv.isEmpty())
                return "";
            if (encoded.length() > MAX_CIPHERTEXT_CHARS || encodedIv.length() > MAX_IV_CHARS)
                throw new IllegalStateException("encrypted API key exceeds storage bounds");
            Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
            byte[] iv = Base64.decode(encodedIv, Base64.NO_WRAP);
            if (iv.length != 12)
                throw new IllegalStateException("invalid encrypted API key IV");
            cipher.init(Cipher.DECRYPT_MODE, key(), new GCMParameterSpec(128, iv));
            byte[] decrypted = cipher.doFinal(Base64.decode(encoded, Base64.NO_WRAP));
            if (decrypted.length > MAX_KEY_BYTES)
                throw new IllegalStateException("decrypted API key exceeds storage bounds");
            return validate(new String(decrypted, StandardCharsets.UTF_8));
        } catch (Exception exception) {
            return "";
        }
    }

    public static synchronized boolean set(String value) {
        try {
            if (value == null || value.isEmpty()) {
                delete();
                return true;
            }
            String validated = validate(value);
            Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
            cipher.init(Cipher.ENCRYPT_MODE, key());
            byte[] encrypted = cipher.doFinal(validated.getBytes(StandardCharsets.UTF_8));
            byte[] iv = cipher.getIV();
            String encoded = Base64.encodeToString(encrypted, Base64.NO_WRAP);
            String encodedIv = Base64.encodeToString(iv, Base64.NO_WRAP);
            if (encoded.length() > MAX_CIPHERTEXT_CHARS || encodedIv.length() > MAX_IV_CHARS)
                return false;
            return preferences().edit()
                    .putString(CIPHERTEXT, encoded)
                    .putString(IV, encodedIv)
                    .commit();
        } catch (Exception exception) {
            return false;
        }
    }

    public static synchronized boolean delete() {
        try {
            KeyStore keyStore = keyStore();
            if (keyStore.containsAlias(ALIAS))
                keyStore.deleteEntry(ALIAS);
            return preferences().edit().clear().commit();
        } catch (Exception exception) {
            return false;
        }
    }

    private static String validate(String value) {
        byte[] encoded = value.getBytes(StandardCharsets.UTF_8);
        if (encoded.length > MAX_KEY_BYTES)
            throw new IllegalArgumentException("API key exceeds storage bounds");
        for (int index = 0; index < value.length(); index++) {
            char character = value.charAt(index);
            if (character < 32 || character == 127)
                throw new IllegalArgumentException("API key contains control characters");
        }
        return value;
    }

    private static SecretKey key() throws Exception {
        KeyStore keyStore = keyStore();
        if (keyStore.containsAlias(ALIAS))
            return ((KeyStore.SecretKeyEntry) keyStore.getEntry(ALIAS, null)).getSecretKey();
        KeyGenerator generator = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore");
        generator.init(new KeyGenParameterSpec.Builder(
                ALIAS,
                KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT)
                .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
                .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
                .setUserAuthenticationRequired(false)
                .build());
        return generator.generateKey();
    }

    private static KeyStore keyStore() throws Exception {
        KeyStore keyStore = KeyStore.getInstance("AndroidKeyStore");
        keyStore.load(null);
        return keyStore;
    }

    private static SharedPreferences preferences() throws Exception {
        Class<?> activityThread = Class.forName("android.app.ActivityThread");
        Method currentApplication = activityThread.getMethod("currentApplication");
        Object application = currentApplication.invoke(null);
        if (!(application instanceof Context))
            throw new IllegalStateException("application context unavailable");
        return application.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
    }
}
