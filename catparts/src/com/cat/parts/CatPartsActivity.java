package com.cat.parts;

import android.app.ActionBar;
import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.PreferenceFragment;
import android.preference.PreferenceManager;
import android.preference.SwitchPreference;
import android.view.MenuItem;
import android.view.View;
import android.widget.ListView;
import java.io.FileWriter;

public class CatPartsActivity extends Activity {

    private static void writeSysfs(String path, String val) {
        try {
            FileWriter fw = new FileWriter(path, false);
            fw.write(val);
            fw.flush();
            fw.close();
        } catch (Exception ignored) {
        }
    }

    public static void syncConfigToDaemon(Context ctx) {
        try {
            Context deCtx = ctx.createDeviceProtectedStorageContext();
            SharedPreferences sp = PreferenceManager.getDefaultSharedPreferences(deCtx);
            String pttShort = sp.getString("ptt_short", "torch");
            String pttLong = sp.getString("ptt_long", "outer_toggle");
            String pttIncall = sp.getString("ptt_incall", "answer_spk");
            boolean outerEnabled = sp.getBoolean("outer_enabled", true);
            String outerFooter = sp.getString("outer_footer", "battery");
            boolean keylightEnabled = sp.getBoolean("keylight_enabled", true);
            String keylightTimeout = sp.getString("keylight_timeout", "0");
            int applyId = sp.getInt("apply_counter", 0) + 1;
            sp.edit().putInt("apply_counter", applyId).commit();

            String effectiveKeylight = keylightEnabled ? "255" : "0";
            String effectiveOuter = outerEnabled ? "255" : "0";

            /* Direct immediate hardware feedback for keypad & outer display ON/OFF */
            writeSysfs("/sys/class/leds/keyboard_light/brightness", effectiveKeylight + "\n");
            writeSysfs("/sys/devices/platform/soc/soc:keyboard_light/leds/keyboard_light/brightness", effectiveKeylight + "\n");
            writeSysfs("/sys/class/leds/lcd-backlight2/brightness", effectiveOuter + "\n");

            StringBuilder sb = new StringBuilder();
            sb.append("ptt_short=").append(pttShort).append("\n");
            sb.append("ptt_long=").append(pttLong).append("\n");
            sb.append("ptt_incall=").append(pttIncall).append("\n");
            sb.append("outer_enabled=").append(outerEnabled ? "1" : "0").append("\n");
            sb.append("outer_brightness=").append(effectiveOuter).append("\n");
            sb.append("outer_footer=").append(outerFooter).append("\n");
            sb.append("keylight_enabled=").append(keylightEnabled ? "1" : "0").append("\n");
            sb.append("keylight_brightness=").append(effectiveKeylight).append("\n");
            sb.append("keylight_timeout=").append(keylightTimeout).append("\n");
            sb.append("apply=").append(applyId).append("\n");

            FileWriter fw = new FileWriter("/dev/shm_outerscreen_cmd", false);
            fw.write(sb.toString());
            fw.flush();
            fw.close();
        } catch (Exception ignored) {
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        ActionBar ab = getActionBar();
        if (ab != null) {
            ab.setDisplayHomeAsUpEnabled(true);
            ab.setTitle("Cat S22 Flip Settings");
        }
        getFragmentManager().beginTransaction()
                .replace(android.R.id.content, new CatPrefsFragment())
                .commit();
        syncConfigToDaemon(this);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == android.R.id.home) {
            finish();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    public static class CatPrefsFragment extends PreferenceFragment
            implements SharedPreferences.OnSharedPreferenceChangeListener {
        @Override
        public void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            getPreferenceManager().setStorageDeviceProtected();
            addPreferencesFromResource(R.xml.cat_preferences);
        }

        @Override
        public void onViewCreated(View view, Bundle savedInstanceState) {
            super.onViewCreated(view, savedInstanceState);
            view.setFitsSystemWindows(true);
            ListView lv = view.findViewById(android.R.id.list);
            if (lv != null) {
                lv.setClipToPadding(false);
                int padTop = (int) (8 * getResources().getDisplayMetrics().density);
                int padBot = (int) (24 * getResources().getDisplayMetrics().density);
                lv.setPadding(lv.getPaddingLeft(), padTop, lv.getPaddingRight(), padBot);
            }
        }

        @Override
        public void onResume() {
            super.onResume();
            SharedPreferences sp = getPreferenceScreen().getSharedPreferences();
            if (sp != null) {
                sp.registerOnSharedPreferenceChangeListener(this);
                SwitchPreference sw = (SwitchPreference) findPreference("outer_enabled");
                if (sw != null) {
                    sw.setChecked(sp.getBoolean("outer_enabled", true));
                }
            }
        }

        @Override
        public void onPause() {
            SharedPreferences sp = getPreferenceScreen().getSharedPreferences();
            if (sp != null) sp.unregisterOnSharedPreferenceChangeListener(this);
            super.onPause();
        }

        @Override
        public void onSharedPreferenceChanged(SharedPreferences sharedPreferences, String key) {
            if ("apply_counter".equals(key)) return;
            if ("outer_enabled".equals(key)) {
                SwitchPreference sw = (SwitchPreference) findPreference("outer_enabled");
                if (sw != null) {
                    sw.setChecked(sharedPreferences.getBoolean("outer_enabled", true));
                }
            }
            syncConfigToDaemon(getActivity());
        }
    }
}
