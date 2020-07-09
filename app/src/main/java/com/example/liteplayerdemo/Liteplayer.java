package com.example.liteplayerdemo;

import android.app.Activity;
import android.content.res.AssetManager;
import android.graphics.SurfaceTexture;
import android.os.Bundle;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.widget.TextView;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.RadioButton;
import android.widget.Spinner;

import java.io.IOException;

public class Liteplayer extends Activity {
    private long mNativeContext; // accessed by native methods

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // Example of a call to a native method
        TextView tv = findViewById(R.id.sample_text);
        tv.setText(stringFromJNI());
    }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    public native void native_create();
    public native void native_setDataSource(String path);
    public native void native_prepareAsync();
    public native void native_start();
    public native void native_pause();
    public native void native_resume();
    public native void native_seekTo(int msec);
    public native void native_stop();
    public native void native_reset();
    public native int native_getCurrentPosition();
    public native int native_getDuration();
    public native void native_destroy();
    public native String stringFromJNI();

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("liteplayer-jni");
    }
}
