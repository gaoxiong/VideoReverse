package com.ffmpegtest;

import java.io.File;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Environment;
import android.support.annotation.NonNull;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.Surface;
import android.view.View;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.EditText;
import android.widget.ListView;

import com.appunite.ffmpeg.NotPlayingException;
import com.ffmpegtest.adapter.ItemsAdapter;
import com.ffmpegtest.adapter.VideoItem;

public class MainActivity extends Activity implements OnItemClickListener {

<<<<<<< HEAD
	private ItemsAdapter adapter;
=======
	private native int initNative();

	private native void deallocNative();

	private native int setDataSourceNative(String url,
																				 Map<String, String> dictionary, int videoStreamNo,
																				 int audioStreamNo, int subtitleStreamNo);

	native void renderFrameStart();

	native void renderFrameStop();

	private native void pauseNative() throws NotPlayingException;

	private native void resumeNative() throws NotPlayingException;

	private native void seekNative(long positionUs) throws NotPlayingException;

	private native long getVideoDurationNative();

	public native void render(Surface surface);

	private native void stopNative();
>>>>>>> e948947... Update to origin code

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.main_activity);

		final ListView listView = (ListView) findViewById(R.id.main_activity_list);
		final EditText editText = (EditText) findViewById(R.id.main_activity_video_url);
		final View button = findViewById(R.id.main_activity_play_button);

		final UserPreferences userPreferences = new UserPreferences(this);
		if (savedInstanceState == null) {
			editText.setText(userPreferences.getUrl());
		}
		adapter = new ItemsAdapter(LayoutInflater.from(this));
		adapter.swapItems(getVideoItems());

		listView.setAdapter(adapter);
		listView.setOnItemClickListener(this);

		button.setOnClickListener(new View.OnClickListener() {
			@Override
			public void onClick(View v) {
				final String url = String.valueOf(editText.getText());
				playVideo(url);
				userPreferences.setUrl(url);
			}
		});
<<<<<<< HEAD
=======

		reverseChooseBtn.setOnClickListener(new View.OnClickListener(){
			@Override
			public void onClick(View v) {
				Intent intent = new Intent(Intent.ACTION_GET_CONTENT);//ACTION_OPEN_DOCUMENT
				intent.addCategory(Intent.CATEGORY_OPENABLE);
				intent.setType("video/*");
				startActivityForResult(intent, 0);
			}
		});

		reversedPlayBtn.setOnClickListener(new View.OnClickListener(){
			@Override
			public void onClick(View v) {
				if (reversedVideoFilePath != "" && isFolderExists(reversedVideoFilePath, false)) {
					try {
						Intent intent = new Intent(Intent.ACTION_VIEW);
						intent.setDataAndType(Uri.parse(reversedVideoFilePath), "video/mp4");
						startActivity(intent);
					} catch (Exception e) {
						Toast.makeText(getApplicationContext(),
							"No player found!", Toast.LENGTH_SHORT).show();
					}
				} else {
					Toast.makeText(getApplicationContext(),
						"Cannot open this video!", Toast.LENGTH_SHORT).show();
				}
			}
		});

		reverseBtn.setOnClickListener(new View.OnClickListener(){
			@Override
			public void onClick(View v) {
				String fileSrc = reverseEditText.getText().toString();
				String filePath = fileSrc.substring(0, fileSrc.lastIndexOf("/") + 1);
				String fileName = fileSrc.substring(fileSrc.lastIndexOf("/") + 1);
				String fileTmp = "/sdcard/tmp";
				String fileDst = filePath + "r_" + fileName;
				if (!isFolderExists(fileTmp, true)) {
					Toast.makeText(getApplicationContext(),
						"Cannot create temp file!", Toast.LENGTH_SHORT).show();
					return;
				}
				if (nativeInit == 0) {
					Toast.makeText(activity,
						"Start reversing...", Toast.LENGTH_SHORT).show();
					reversedVideoFilePath = fileDst;
					new ReverseTask(activity).execute(fileSrc, fileDst,
						Long.valueOf(0), Long.valueOf(0),
						Integer.valueOf(1), Integer.valueOf(0), Integer.valueOf(0));
				} else {
					Toast.makeText(getApplicationContext(),
						"Native code not init!", Toast.LENGTH_SHORT).show();
				}
			}
		});
	}

	private static class ReverseTask extends
		AsyncTask<Object, Void, Integer> {

		private final MainActivity activity;

		public ReverseTask(MainActivity activity) {
			this.activity = activity;
		}

		@Override
		protected Integer doInBackground(Object... params) {
			String file_src = (String) params[0];
			String file_dest = (String) params[1];

			long startTime = (Long) params[2];
			long endTime = (Long) params[3];

			Integer videoStream = (Integer) params[4];
			Integer audioStream = (Integer) params[5];
			Integer subtitleStream = (Integer) params[6];

			int videoStreamNo = videoStream == null ? -1 : videoStream.intValue();
			int audioStreamNo = audioStream == null ? -1 : audioStream.intValue();
			int subtitleStreamNo = subtitleStream == null ? -1 : subtitleStream.intValue();
			return 0;

//			int result = activity.reverseNative(file_src, file_dest, startTime, endTime,
//				videoStreamNo, audioStreamNo, subtitleStreamNo);
//
//			return result;
		}

		@Override
		protected void onPostExecute(Integer result) {
			if (result >= 0) {
				Toast.makeText(activity.getApplicationContext(),
					"Reverse DONE!", Toast.LENGTH_SHORT).show();
			} else {
				Toast.makeText(activity.getApplicationContext(),
					"Reverse ERROR!", Toast.LENGTH_SHORT).show();
			}
		}
	}

	private boolean isFolderExists(String strFolder, boolean bCreate) {
		File file = new File(strFolder);
		if (!file.exists()) {
			if (!bCreate) {
				return false;
			}
			if (file.mkdirs()) {
				return true;
			} else {
				return false;
			}
		}
		return true;
	}

	@Override
	protected void onActivityResult(int requestCode, int resultCode, Intent data) {
		if (requestCode == 0 && data != null) {
			Log.d("reverse", "data.getData: " + data.getData());
			Log.d("reverse", "data.getData.getEncodedPath: " + data.getData().getEncodedPath());
			Log.d("reverse", "data.getData.getPath: " + data.getData().getPath());
			reverseEditText.setText(getRealPathFromURI(data.getData()));
		}
	}

	private String getRealPathFromURI(Uri contentUri) {
		if(DocumentsContract.isDocumentUri(getApplicationContext(), contentUri)){
			String wholeID = DocumentsContract.getDocumentId(contentUri);
			String id = wholeID.split(":")[1];
			String[] column = { MediaStore.Video.Media.DATA };
			String sel = MediaStore.Video.Media._ID + "=?";
			String filePath = "";
			Cursor cursor = getContentResolver().query(MediaStore.Video.Media.EXTERNAL_CONTENT_URI,
				column, sel, new String[] { id }, null);
			int columnIndex = cursor.getColumnIndex(column[0]);
			if (cursor.moveToFirst()) {
				filePath = cursor.getString(columnIndex);
			}
			cursor.close();
			return filePath;
		} else {
			String[] proj = {MediaStore.Video.Media.DATA};
			Cursor cursor = managedQuery(contentUri, proj, null, null, null);
			int column_index = cursor.getColumnIndexOrThrow(MediaStore.Video.Media.DATA);
			cursor.moveToFirst();
			return cursor.getString(column_index);
		}
>>>>>>> e948947... Update to origin code
	}

	private void playVideo(String url) {
		final Intent intent = new Intent(AppConstants.VIDEO_PLAY_ACTION)
                .putExtra(AppConstants.VIDEO_PLAY_ACTION_EXTRA_URL, url);
		startActivity(intent);
	}

	@NonNull
	private List<VideoItem> getVideoItems() {
		final List<VideoItem> items = new ArrayList<VideoItem>();
		items.add(new VideoItem(
				items.size(),
				"\"localfile.mp4\" on sdcard",
				getSDCardFile("localfile.mp4"),
				null));
		items.add(new VideoItem(
				items.size(),
				"Apple sample",
				"http://devimages.apple.com.edgekey.net/resources/http-streaming/examples/bipbop_4x3/bipbop_4x3_variant.m3u8",
				null));
		items.add(new VideoItem(
				items.size(),
				"Apple advenced sample",
				"https://devimages.apple.com.edgekey.net/resources/http-streaming/examples/bipbop_16x9/bipbop_16x9_variant.m3u8",
				null));
		items.add(new VideoItem(
				items.size(),
				"IP camera",
				"rtsp://ip.inter.appunite.net:554",
				null));
		return items;
	}

	private static String getSDCardFile(String file) {
		File videoFile = new File(Environment.getExternalStorageDirectory(), file);
		return "file://" + videoFile.getAbsolutePath();
	}

	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		getMenuInflater().inflate(R.menu.main_activity, menu);
		return true;
	}

	@Override
	public void onItemClick(AdapterView<?> listView, View view, int position, long id) {
		final VideoItem videoItem = adapter.getItem(position);
		final Intent intent = new Intent(AppConstants.VIDEO_PLAY_ACTION)
				.putExtra(AppConstants.VIDEO_PLAY_ACTION_EXTRA_URL, videoItem.video())
				.putExtra(AppConstants.VIDEO_PLAY_ACTION_EXTRA_ENCRYPTION_KEY, videoItem.video());
		startActivity(intent);
	}

}
