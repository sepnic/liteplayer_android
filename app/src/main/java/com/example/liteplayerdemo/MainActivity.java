package com.example.liteplayerdemo;

import android.app.Activity;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.util.Log;
import android.widget.TextView;

public class MainActivity extends Activity {
    private final static String TAG = "LitelayerDemo";
    private Liteplayer mLiteplayer;
    private boolean mStarted;
    private boolean mPaused;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        TextView tv = findViewById(R.id.sample_text);
        tv.setText("Liteplayer demo");

        mLiteplayer = new Liteplayer();
        mLiteplayer.setOnIdleListener(mIdleListener);
        mLiteplayer.setOnPreparedListener(mPreparedListener);
        mLiteplayer.setOnStartedListener(mStartedListener);
        mLiteplayer.setOnPausedListener(mPausedListener);
        mLiteplayer.setOnSeekCompletedListener(mSeekCompletedListener);
        mLiteplayer.setOnCompletedListener(mCompletedListener);
        mLiteplayer.setOnErrorListener(mErrorListener);

        // test begin
        mLiteplayer.setDataSource("http://ailabsaicloudservice.alicdn.com/player/resources/23a2d715f019c0e345235f379fa26a30.mp3");
        mLiteplayer.prepareAsync();
    }

    /** Called when the activity is about to be destroyed. */
    @Override
    protected void onDestroy() {
        mLiteplayer.release();
        super.onDestroy();
    }

    @Override
    protected void onPause() {
        if (mStarted && !mPaused)
            mLiteplayer.pause();
        super.onPause();
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (mStarted && mPaused)
            mLiteplayer.resume();
    }

    Liteplayer.OnIdleListener mIdleListener = new Liteplayer.OnIdleListener() {
        public void onIdle(Liteplayer p) {
            mStarted = false;
            mPaused = false;
        }
    };

    Liteplayer.OnPreparedListener mPreparedListener = new Liteplayer.OnPreparedListener() {
        public void onPrepared(Liteplayer p) {
            p.start();
        }
    };

    Liteplayer.OnStartedListener mStartedListener = new Liteplayer.OnStartedListener() {
        public void onStarted(Liteplayer p) {
            mStarted = true;
            mPaused = false;
        }
    };

    Liteplayer.OnPausedListener mPausedListener = new Liteplayer.OnPausedListener() {
        public void onPaused(Liteplayer p) {
            mPaused = true;
        }
    };

    Liteplayer.OnSeekCompletedListener mSeekCompletedListener = new Liteplayer.OnSeekCompletedListener() {
        public void onSeekCompleted(Liteplayer p) {
            if (mPaused == false)
                p.start();
        }
    };

    Liteplayer.OnCompletedListener mCompletedListener = new Liteplayer.OnCompletedListener() {
        public void onCompleted(Liteplayer p) {
            p.reset();
        }
    };

    Liteplayer.OnErrorListener mErrorListener = new Liteplayer.OnErrorListener() {
        public void onError(Liteplayer p, int what, int extra) {
            p.reset();
        }
    };
}
