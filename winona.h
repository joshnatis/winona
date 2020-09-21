/* WINONA AUDIO PLAYER
 * Written by Josh Natis
 *
 * dr_libs and miniaudio libraries courtesy of David Reid (github.com/mackron)
 */

/* -- TODO --
 * FEATURE: UTF8 titles
 * FEATURE: seek forward
	- decoder.outputSampleRate, sample rate * number of seconds
 * FEATURE: allow user to change default directory
 	- either with setup script, config file, or manually I guess

 * IMPROVEMENT: allow (q) and (l) to be recognized instantly when entering input
 	- currently getstr() is used, but that requires clicking enter to confirm
 	- write an input routine that acts automatically on (q) and (l), otherwise
 		accepting input and acting properly on delete key

 * BUG: rarely, play_song() is not called when current song finishes
 	- currently the strategy is to timeout getch() after (duration + 1) seconds,
 		at which point the event loop checks if the song ended. this should always
 		be true at this point unless duration is off by >=1 seconds) or something
 		unsual has happened (like the computer is put to sleep)
 	- consider calling await_command from handle_command in else clause instead of
 		returning to event loop
 		+ this would allow me to simply write winona.play_song(); after await_command()
 			in the event loop, which addresses the issue of play_song() not being
 			executed when duration_ is slightly off thus causing the timeout to trigger
 			earlier than the song ends

 * PERFORMANCE: reduce number of refresh() calls
 	- consider separate windows for acii art and menu
 * PERFORMANCE: profile the program (valgrind, gprof, ...?)
 */

#define DR_FLAC_IMPLEMENTATION
#include "dr_flac.h"

#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <dirent.h> /* To scan for audio files in user-provided directory. load_songs() */
#include <ncurses.h>
#include <time.h> /* To seed the random algorithm for SHUFFLE mode. */

#include <string>
#include <vector>
#include <deque>

#define EXIT 1

using namespace std;

enum mode {SHUFFLE, LINEAR, LOOP, QUEUE};

class AudioPlayer
{
public:
	AudioPlayer(string dir = ".");
	~AudioPlayer();

	void display_menu();
	bool await_command();
	void play_song(int index = -1);
	void view_songs();

private:
	//determined upon startup
	string directory_;
	int num_songs_;
	vector<string> songs_;

	//state
	vector<int> history_;
	deque<int> queue_;
	mode mode_;
	int current_song_;
	bool paused_;
	int duration_;

	//audio and graphics
	ma_decoder decoder_;
	ma_device device_;
	WINDOW *win_;

	void load_songs();
	int get_next_song();
	void handle_command(char option);
	void play_song_boilerplate(string &path);

	void user_pick_song(const string& action = "Play");
	void user_pick_mode();
	void user_pick_queue_option();

	void display_history();
	void display_queue();
	void display_duration();
	void display_mode();
	void display_mp3_player();

	string get_file_ext(const string &fn);
	string to_lower(const string &str);
	bool isnum(const string &str);
	void die(string msg, int err = 1);
};
