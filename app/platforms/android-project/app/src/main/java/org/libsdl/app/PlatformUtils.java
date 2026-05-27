package org.libsdl.app;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.Color;
import android.media.AudioAttributes;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkInfo;
import android.net.Uri;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.os.BatteryManager;
import android.os.Build;
import android.os.Looper;
import android.os.Message;
import android.os.VibrationAttributes;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.provider.Settings;
import android.util.DisplayMetrics;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.widget.RelativeLayout;

public class PlatformUtils {

    private static Vibrator vibrator;
    private static boolean rumbleDisabled;
    private static RelativeLayout mediaCodecSurfaceContainer;
    private static SurfaceView mediaCodecSurfaceView;
    private static volatile Surface mediaCodecSurface;
    private static int mediaCodecVideoWidth;
    private static int mediaCodecVideoHeight;
    private static boolean mediaCodecSurfaceLayoutUpdatePosted;
    private static boolean mediaCodecSurfaceUsesFixedSize;
    private static int mediaCodecSurfaceBufferWidth = -1;
    private static int mediaCodecSurfaceBufferHeight = -1;

    private static void requestMediaCodecVideoSurfaceLayoutUpdate() {
        if (mediaCodecSurfaceContainer == null || mediaCodecSurfaceView == null) {
            return;
        }

        if (Looper.myLooper() != Looper.getMainLooper()) {
            Context context = SDLActivity.getContext();
            if (context instanceof Activity) {
                ((Activity) context).runOnUiThread(PlatformUtils::requestMediaCodecVideoSurfaceLayoutUpdate);
            }
            return;
        }

        if (mediaCodecSurfaceLayoutUpdatePosted) {
            return;
        }

        mediaCodecSurfaceLayoutUpdatePosted = true;
        mediaCodecSurfaceContainer.post(() -> {
            mediaCodecSurfaceLayoutUpdatePosted = false;
            updateMediaCodecVideoSurfaceLayout();
        });
    }

    private static void updateMediaCodecVideoSurfaceLayout() {
        if (mediaCodecSurfaceContainer == null || mediaCodecSurfaceView == null) {
            return;
        }

        int containerWidth = mediaCodecSurfaceContainer.getWidth();
        int containerHeight = mediaCodecSurfaceContainer.getHeight();

        if (containerWidth <= 0 || containerHeight <= 0) {
            Context context = SDLActivity.getContext();
            if (context == null) {
                return;
            }

            DisplayMetrics metrics = new DisplayMetrics();
            WindowManager windowManager =
                    (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);
            if (windowManager == null) {
                return;
            }

            windowManager.getDefaultDisplay().getRealMetrics(metrics);
            containerWidth = metrics.widthPixels;
            containerHeight = metrics.heightPixels;
        }

        int targetWidth = containerWidth;
        int targetHeight = containerHeight;

        if (mediaCodecVideoWidth > 0 && mediaCodecVideoHeight > 0) {
            float videoAspect = (float) mediaCodecVideoWidth / (float) mediaCodecVideoHeight;
            float containerAspect = (float) containerWidth / (float) containerHeight;

            if (videoAspect > containerAspect) {
                targetWidth = containerWidth;
                targetHeight = Math.round(containerWidth / videoAspect);
            }
            else {
                targetHeight = containerHeight;
                targetWidth = Math.round(containerHeight * videoAspect);
            }
        }

        RelativeLayout.LayoutParams layoutParams =
                (RelativeLayout.LayoutParams) mediaCodecSurfaceView.getLayoutParams();
        boolean layoutChanged = false;
        if (layoutParams == null) {
            layoutParams = new RelativeLayout.LayoutParams(targetWidth, targetHeight);
            layoutParams.addRule(RelativeLayout.CENTER_IN_PARENT, RelativeLayout.TRUE);
            layoutChanged = true;
        }

        if (layoutParams.width != targetWidth || layoutParams.height != targetHeight) {
            layoutParams.width = targetWidth;
            layoutParams.height = targetHeight;
            layoutChanged = true;
        }

        if (layoutChanged) {
            mediaCodecSurfaceView.setLayoutParams(layoutParams);
        }

        if (mediaCodecVideoWidth > 0 && mediaCodecVideoHeight > 0) {
            if (!mediaCodecSurfaceUsesFixedSize ||
                    mediaCodecSurfaceBufferWidth != mediaCodecVideoWidth ||
                    mediaCodecSurfaceBufferHeight != mediaCodecVideoHeight) {
                mediaCodecSurfaceUsesFixedSize = true;
                mediaCodecSurfaceBufferWidth = mediaCodecVideoWidth;
                mediaCodecSurfaceBufferHeight = mediaCodecVideoHeight;
                mediaCodecSurfaceView.getHolder().setFixedSize(mediaCodecVideoWidth, mediaCodecVideoHeight);
            }
        }
        else if (mediaCodecSurfaceUsesFixedSize) {
            mediaCodecSurfaceUsesFixedSize = false;
            mediaCodecSurfaceBufferWidth = -1;
            mediaCodecSurfaceBufferHeight = -1;
            mediaCodecSurfaceView.getHolder().setSizeFromLayout();
        }
    }

    public static void installMediaCodecVideoSurface(SDLActivity activity,
                                                     ViewGroup layout,
                                                     SDLSurface sdlSurface) {
        if (!(layout instanceof RelativeLayout)) {
            return;
        }

        RelativeLayout relativeLayout = (RelativeLayout) layout;

        if (mediaCodecSurfaceContainer != null) {
            if (mediaCodecSurfaceContainer.getParent() instanceof ViewGroup) {
                ((ViewGroup) mediaCodecSurfaceContainer.getParent())
                        .removeView(mediaCodecSurfaceContainer);
            }

            relativeLayout.addView(mediaCodecSurfaceContainer, 0);
            sdlSurface.enableMediaCodecOverlay();
            requestMediaCodecVideoSurfaceLayoutUpdate();
            return;
        }

        mediaCodecSurfaceContainer = new RelativeLayout(activity);
        mediaCodecSurfaceContainer.setLayoutParams(new RelativeLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT));
        mediaCodecSurfaceContainer.setBackgroundColor(Color.BLACK);

        mediaCodecSurfaceView = new SurfaceView(activity);
        mediaCodecSurfaceView.setFocusable(false);
        mediaCodecSurfaceView.setClickable(false);

        RelativeLayout.LayoutParams surfaceLayoutParams = new RelativeLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT);
        surfaceLayoutParams.addRule(RelativeLayout.CENTER_IN_PARENT, RelativeLayout.TRUE);
        mediaCodecSurfaceContainer.addView(mediaCodecSurfaceView, surfaceLayoutParams);
        mediaCodecSurfaceView.getHolder().addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(SurfaceHolder holder) {
                mediaCodecSurface = holder.getSurface();
            }

            @Override
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
                mediaCodecSurface = holder.getSurface();
            }

            @Override
            public void surfaceDestroyed(SurfaceHolder holder) {
                mediaCodecSurface = null;
            }
        });

        mediaCodecSurfaceContainer.addOnLayoutChangeListener((view, left, top, right, bottom,
                                                              oldLeft, oldTop, oldRight, oldBottom) -> {
            if (left == oldLeft && top == oldTop && right == oldRight && bottom == oldBottom) {
                return;
            }

            requestMediaCodecVideoSurfaceLayoutUpdate();
        });

        relativeLayout.addView(mediaCodecSurfaceContainer);
        sdlSurface.enableMediaCodecOverlay();
        requestMediaCodecVideoSurfaceLayoutUpdate();
    }

    public static Surface getMediaCodecVideoSurface() {
        if (mediaCodecSurface == null || !mediaCodecSurface.isValid()) {
            return null;
        }

        return mediaCodecSurface;
    }

    public static void setMediaCodecVideoSurfaceContentSize(int width, int height) {
        mediaCodecVideoWidth = width;
        mediaCodecVideoHeight = height;

        Context context = SDLActivity.getContext();
        if (context instanceof Activity) {
            ((Activity) context).runOnUiThread(PlatformUtils::requestMediaCodecVideoSurfaceLayoutUpdate);
        }
        else {
            requestMediaCodecVideoSurfaceLayoutUpdate();
        }
    }

    public static boolean isBatterySupported() {
        Context context = SDLActivity.getContext();
        Intent batteryIntent = context.registerReceiver(null, new IntentFilter(Intent.ACTION_BATTERY_CHANGED));
        return batteryIntent != null;
    }

    public static int getBatteryLevel() {
        Context context = SDLActivity.getContext();

        Intent batteryIntent = context.registerReceiver(null, new IntentFilter(Intent.ACTION_BATTERY_CHANGED));
        if (batteryIntent == null) {
            return 0;
        }
        int level = batteryIntent.getIntExtra(BatteryManager.EXTRA_LEVEL, -1);
        int scale = batteryIntent.getIntExtra(BatteryManager.EXTRA_SCALE, -1);

        if (level >= 0 && scale > 0) {
            return (level * 100) / scale;
        }

        return 0;
    }

    public static boolean isBatteryCharging() {
        Context context = SDLActivity.getContext();

        IntentFilter filter = new IntentFilter(Intent.ACTION_BATTERY_CHANGED);
        Intent batteryStatus = context.registerReceiver(null, filter);

        int status = batteryStatus.getIntExtra(BatteryManager.EXTRA_STATUS, -1);
        return status == BatteryManager.BATTERY_STATUS_CHARGING ||
                status == BatteryManager.BATTERY_STATUS_FULL;
    }

    public static boolean isEthernetConnected() {
        Context context = SDLActivity.getContext();

        ConnectivityManager connectivityManager = (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
        Network[] networks = connectivityManager.getAllNetworks();
        for (Network network : networks) {
            NetworkCapabilities capabilities = connectivityManager.getNetworkCapabilities(network);
            if (capabilities != null && capabilities.hasTransport(NetworkCapabilities.TRANSPORT_ETHERNET)) {
                return true;
            }
        }
        return false;
    }

    public static boolean isWifiSupported() {
        Context context = SDLActivity.getContext();

        WifiManager wifiManager = (WifiManager) context.getApplicationContext().getSystemService(Context.WIFI_SERVICE);
        return wifiManager != null && wifiManager.isWifiEnabled();
    }

    public static boolean isWifiConnected() {
        Context context = SDLActivity.getContext();

        ConnectivityManager connectivityManager = (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
        NetworkInfo wifiInfo = connectivityManager.getNetworkInfo(ConnectivityManager.TYPE_WIFI);
        return wifiInfo != null && wifiInfo.isConnected();
    }

    public static int getWifiSignalStrength() {
        Context context = SDLActivity.getContext();

        WifiManager wifiManager = (WifiManager) context.getApplicationContext().getSystemService(Context.WIFI_SERVICE);
        WifiInfo wifiInfo = wifiManager.getConnectionInfo();
        return wifiInfo.getRssi();
    }

    public static void openBrowser(String url) {
        Context context = SDLActivity.getContext();

        Uri webpage = Uri.parse(url);
        Intent intent = new Intent(Intent.ACTION_VIEW, webpage);
        if (intent.resolveActivity(context.getPackageManager()) != null) {
            context.startActivity(intent);
        }
    }

    public static float getSystemScreenBrightness(Context context) {
        ContentResolver contentResolver = context.getContentResolver();
        return Settings.System.getInt(contentResolver,
                Settings.System.SCREEN_BRIGHTNESS, 125) * 1.0f / 255.0f;
    }

    public static BorealisHandler borealisHandler = null;

    public static void setAppScreenBrightness(Activity activity, float value) {
        Message message = Message.obtain();
        message.obj = activity;
        message.arg1 = (int)(value * 255);
        message.what = 0;
        if(borealisHandler != null) borealisHandler.sendMessage(message);
    }

    public static float getAppScreenBrightness(Activity activity) {
        Window window = activity.getWindow();
        WindowManager.LayoutParams lp = window.getAttributes();
        if (lp.screenBrightness < 0) return getSystemScreenBrightness(activity);
        return lp.screenBrightness;
    }

    public static void initDeviceRumble() {
        Context context = SDLActivity.getContext();
        vibrator = (Vibrator) context.getSystemService(Context.VIBRATOR_SERVICE);
        rumbleDisabled = (vibrator == null || !vibrator.hasVibrator());
//        if (rumbleDisabled && BuildConfig.DEBUG)
//        {
//            Log.w("Rumble", "System does not have a Vibrator, or the permission is disabled. " +
//                    "Rumble has been turned rest. Subsequent calls to static methods will have no effect.");
//        }
    }

    public static void deviceRumble(short lowFreqMotor, short highFreqMotor) {
        if (rumbleDisabled) return;

        Context context = SDLActivity.getContext();

        short lowFreqMotorAdjusted = (short)(Math.min(lowFreqMotor, 0xFF));
        short highFreqMotorAdjusted = (short)(Math.min(highFreqMotor, 0xFF));

        rumbleSingleVibrator(vibrator, lowFreqMotorAdjusted, highFreqMotorAdjusted);
    }

    private static void rumbleSingleVibrator(Vibrator vibrator, short lowFreqMotor, short highFreqMotor) {
        // Since we can only use a single amplitude value, compute the desired amplitude
        // by taking 80% of the big motor and 33% of the small motor, then capping to 255.
        // NB: This value is now 0-255 as required by VibrationEffect.
//        short lowFreqMotorMSB = (short)((lowFreqMotor >> 8) & 0xFF);
//        short highFreqMotorMSB = (short)((highFreqMotor >> 8) & 0xFF);
        int simulatedAmplitude = Math.min(255, (int)((lowFreqMotor * 0.80) + (highFreqMotor * 0.33)));

        if (simulatedAmplitude == 0) {
            // This case is easy - just cancel the current effect and get out.
            // NB: We cannot simply check lowFreqMotor == highFreqMotor == 0
            // because our simulatedAmplitude could be 0 even though our inputs
            // are not (ex: lowFreqMotor == 0 && highFreqMotor == 1).
            vibrator.cancel();
            return;
        }

        // Attempt to use amplitude-based control if we're on Oreo and the device
        // supports amplitude-based vibration control.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            if (vibrator.hasAmplitudeControl()) {
                VibrationEffect effect = VibrationEffect.createOneShot(60000, simulatedAmplitude);
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                    VibrationAttributes vibrationAttributes = new VibrationAttributes.Builder()
                            .setUsage(VibrationAttributes.USAGE_MEDIA)
                            .build();
                    vibrator.vibrate(effect, vibrationAttributes);
                }
                else {
                    AudioAttributes audioAttributes = new AudioAttributes.Builder()
                            .setUsage(AudioAttributes.USAGE_GAME)
                            .build();
                    vibrator.vibrate(effect, audioAttributes);
                }
                return;
            }
        }

        // If we reach this point, we don't have amplitude controls available, so
        // we must emulate it by PWMing the vibration. Ick.
        long pwmPeriod = 20;
        long onTime = (long)((simulatedAmplitude / 255.0) * pwmPeriod);
        long offTime = pwmPeriod - onTime;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            VibrationAttributes vibrationAttributes = new VibrationAttributes.Builder()
                    .setUsage(VibrationAttributes.USAGE_MEDIA)
                    .build();
            vibrator.vibrate(VibrationEffect.createWaveform(new long[]{0, onTime, offTime}, 0), vibrationAttributes);
        }
        else {
            AudioAttributes audioAttributes = new AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_GAME)
                    .build();
            vibrator.vibrate(new long[]{0, onTime, offTime}, 0, audioAttributes);
        }
    }
}