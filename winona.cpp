#include "winona.h"

ma_uint64 FRAMES_READ = -1;

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
	ma_decoder* pDecoder = (ma_decoder*)pDevice->pUserData;
	if (pDecoder == NULL)
	    return;

	FRAMES_READ = ma_decoder_read_pcm_frames(pDecoder, pOutput, frameCount);
	(void)pInput;
}

/*_____   _    _  ____   _       _____  _____ 
 |  __ \ | |  | ||  _ \ | |     |_   _|/ ____|
 | |__) || |  | || |_) || |       | | | |     
 |  ___/ | |  | ||  _ < | |       | | | |     
 | |     | |__| || |_) || |____  _| |_| |____ 
 |_|      \____/ |____/ |______||_____|\_____|*/

//modifies directory_, num_songs_, mode_, paused_, win_,
AudioPlayer::AudioPlayer(string dir): 
	directory_(dir), num_songs_(0), mode_(SHUFFLE), paused_(false)
{
	win_ = newwin(80, 24, 0, 0);
	srand(time(NULL));
	load_songs(); //fills songs_ vector with audio files found in dir
	play_song();
}

//modifies device_, decoder_
AudioPlayer::~AudioPlayer()
{
	//deallocate dynamic memory used by miniaudio
	ma_device_uninit(&device_);
	ma_decoder_uninit(&decoder_);
}

void AudioPlayer::display_menu()
{
	display_mp3_player();
	display_mode();
	printw("(n)ext, (b)ack, (l)ibrary, (p)ick, (q)uit");
	
	if(paused_) printw(", (s)tart\n");
	else printw(", (s)top\n");

	printw("(r)estart, (M)ode, (Q)ueue\n");
}

bool AudioPlayer::await_command()
{
	int option = getch();
	if(option == '\r' || option == '\n') return !EXIT;

	if(option == 'q') return EXIT;
	else
	{
		handle_command(option);
		return !EXIT;
	}
}

//modifies current_songs_, history_, device_, decoder_
void AudioPlayer::play_song(int index)
{
	//deallocate memory for previous song
	ma_device_uninit(&device_);
	ma_decoder_uninit(&decoder_);

	//fetch next song. either determined by index, or, if -1, a new song is chosen (based on mode)
	current_song_ = (index != -1) ? index : get_next_song();

	//add the song to history (as long as it isn't being replayed)
	if(history_.size() == 0 || history_[history_.size() - 1] != current_song_)
		history_.push_back(current_song_);

	string path = directory_ + "/" + songs_[current_song_];

	//call miniaudio library functions to play audio file
	play_song_boilerplate(path);
}

void AudioPlayer::view_songs()
{
	string view = "";
	for(int i = 0; i < num_songs_; ++i)
		view += "[" + to_string(i) + "]\t" + songs_[i] + "\n";

	clear(); endwin();
	//use the 'less' pager to view songs and their index
	system(("tput bold; echo \"" + view + "\" | less -S; tput sgr0").c_str());
	win_ = newwin(80, 24, 0, 0);
}

/*_____   _____   _____ __      __    _______  ______ 
 |  __ \ |  __ \ |_   _|\ \    / //\ |__   __||  ____|
 | |__) || |__) |  | |   \ \  / //  \   | |   | |__   
 |  ___/ |  _  /   | |    \ \/ // /\ \  | |   |  __|  
 | |     | | \ \  _| |_    \  // ____ \ | |   | |____ 
 |_|     |_|  \_\|_____|    \//_/    \_\|_|   |______|*/

//modifies mode_
int AudioPlayer::get_next_song()
{
	if(mode_ == SHUFFLE) return rand() % num_songs_;
	else if(mode_ == LINEAR) return (current_song_ + 1) % num_songs_;
	else if(mode_ == LOOP) return current_song_;
	else if(mode_ == QUEUE)
	{
		if(queue_.size() == 0)
		{
			mode_ = SHUFFLE;
			return rand() % num_songs_;
		}
		else
		{
			int next = queue_.front();
			queue_.pop_front();
			if(queue_.size() == 0) mode_ = SHUFFLE;
			return next;
		}
	}
	else
	{
		die("Unable to determine next song");
		return -1;
	}
}

//modifies songs_ and num_songs_
void AudioPlayer::load_songs()
{
	DIR *d = opendir(directory_.c_str());
	struct dirent *dir;
	if(d)
	{
		string filename, ext;
		while ((dir = readdir(d)) != NULL)
		{
			filename = dir->d_name;
			ext = get_file_ext(filename);
			if(ext == "mp3" || ext == "wav" || ext == "flac")
			{
				songs_.push_back(filename);
				num_songs_++;
			}
		}
		closedir(d);
	}

	if(num_songs_ == 0)
		die("No songs found at " + directory_);
}

//modifies history_, paused_
void AudioPlayer::handle_command(char option)
{
	if(option == 'n') play_song();
	else if(option == 'p') user_pick_song();
	else if(option == 'r') play_song(current_song_);
	else if(option == 'M') user_pick_mode();
	else if(option == 'h') display_history();
	else if(option == 'd') display_duration();
	else if(option == 'l') view_songs();
	else if(option == 'Q') user_pick_queue_option();
	else if(option == 'b')
	{
		if(history_.size() > 1)
		{
			int prev_index = history_[history_.size() - 2];
			history_.pop_back();
			play_song(prev_index);
		}
	}
	else if(option == 's')
	{
		if(!paused_) ma_device_stop(&device_);
		else ma_device_start(&device_);
		paused_ = !paused_;
	}
}

//modifies duration_
void AudioPlayer::play_song_boilerplate(string &path)
{
	ma_result result;
	ma_device_config deviceConfig;

	result = ma_decoder_init_file((path).c_str(), NULL, &decoder_);
	if (result != MA_SUCCESS)
		die("Unable to open file " + path);

	deviceConfig = ma_device_config_init(ma_device_type_playback);
	deviceConfig.playback.format   = decoder_.outputFormat;
	deviceConfig.playback.channels = decoder_.outputChannels;
	deviceConfig.sampleRate        = decoder_.outputSampleRate;
	deviceConfig.dataCallback      = data_callback;
	deviceConfig.pUserData         = &decoder_;


	if (ma_device_init(NULL, &deviceConfig, &device_) != MA_SUCCESS)
	{
		ma_decoder_uninit(&decoder_);
		die("Failed to open playback device");
	}

	if (ma_device_start(&device_) != MA_SUCCESS)
	{
		ma_device_uninit(&device_);
		ma_decoder_uninit(&decoder_);
		die("Failed to start playback device");
	}

	/* calculate the duration of the current song in seconds */
	duration_ = ma_decoder_get_length_in_pcm_frames(&decoder_) / (decoder_.outputSampleRate);
	/* only block getch() for duration of song (ms), after release next song will play */
	timeout((duration_ + 1) * 1000);
}

/*_    _   _____  ______  _____     _____  _   _  _____   _    _  _______ 
 | |  | | / ____||  ____||  __ \   |_   _|| \ | ||  __ \ | |  | ||__   __|
 | |  | || (___  | |__   | |__) |    | |  |  \| || |__) || |  | |   | |   
 | |  | | \___ \ |  __|  |  _  /     | |  | . ` ||  ___/ | |  | |   | |   
 | |__| | ____) || |____ | | \ \    _| |_ | |\  || |     | |__| |   | |   
  \____/ |_____/ |______||_|  \_\  |_____||_| \_||_|      \____/    |_|*/  

void AudioPlayer::user_pick_song(const string &action)
{
	echo(); curs_set(1);
	clear(); display_mp3_player(); display_mode();
	printw("(q)uit, (l)ibrary\n|| %s song #: ", action.c_str()); refresh();

	char index[7]; // 7 digit number mate (avoiding int overflow)
	getnstr(index, 7);

	if(*index == '\0') return;
	else if(!strcmp(index, "q")) {}
	else if(!strcmp(index, "l"))
	{
		view_songs();
		user_pick_song(action);
	}
	else if(isnum(index) && stoi(index) < num_songs_)
	{
		if(action == "Play") play_song(stoi(index));
		else if(action == "Queue")
		{
			mode_ = QUEUE;
			queue_.push_back(stoi(index));
		}
	}
	else
	{
		printw("Invalid index."); refresh();
		sleep(1);
	}

	noecho(); curs_set(0);
}

//modifies mode_
void AudioPlayer::user_pick_mode()
{
	clear(); display_mp3_player(); display_mode();
	printw("(s)huffle, (l)oop, (L)inear, (Q)ueue, (q)uit\n"); refresh();

	char option = getch();
	if(option == 's') mode_ = SHUFFLE;
	else if(option == 'l') mode_ = LOOP;
	else if(option == 'L') mode_ = LINEAR;
	else if(option == 'Q') mode_ = QUEUE;
	else if(option == 'q') return;
	else
	{
		printw("Invalid option. (%c)\n", option); refresh();
		sleep(1);
	}
}

void AudioPlayer::user_pick_queue_option()
{
	clear(); display_mp3_player(); display_mode();
	printw("(a)dd to queue, (v)iew queue, (q)uit\n"); refresh();

	char option = getch();
	if(option == 'a') user_pick_song("Queue");
	else if(option == 'v') display_queue();
	else if(option == 'q') return;
	else
	{
		printw("Invalid option. (%c)\n", option); refresh();
		sleep(1);
	}
}

/*_____  _____   _____  _____   _             __     __
 |  __ \|_   _| / ____||  __ \ | |         /\ \ \   / /
 | |  | | | |  | (___  | |__) || |        /  \ \ \_/ / 
 | |  | | | |   \___ \ |  ___/ | |       / /\ \ \   /  
 | |__| |_| |_  ____) || |     | |____  / ____ \ | |   
 |_____/|_____||_____/ |_|     |______|/_/    \_\|_|*/   
                                                       
void AudioPlayer::display_mp3_player()
{
	printw("  ____________________________\n"
	" /|............................|\n"
	"| |:                          :|\n"
	"| |:  %s\n"
	"| |:     ,-.   _____   ,-.    :|\n"
	"| |:    ( `)) [_____] ( `))   :|\n"
	"|v|:     `-`   ' ' '   `-`    :|\n"
	"|||:     ,______________.     :|\n"
	"|||...../::::o::::::o::::\\.....|\n"
	"|^|..../:::O::::::::::O:::\\....|\n"
	"|/`---/--------------------`---|\n"
	"`.___/ /====/ /=//=/ /====/____/\n"
	"     `--------------------'\n", songs_[current_song_].c_str());
}

void AudioPlayer::display_mode()
{
	printw("           MODE: ");
	if(mode_ == LOOP) printw("LOOP\n\n");
	else if(mode_ == SHUFFLE) printw("SHUFFLE\n\n");
	else if(mode_ == LINEAR) printw("LINEAR\n\n");
	else if(mode_ == QUEUE) printw("QUEUE\n\n");
}

void AudioPlayer::display_history()
{
	printw("[");
	int len = history_.size();
	for(int i = 0; i < len - 1; ++i)
		printw("%d, ", history_[i]);
	printw("%d]\nPress any key to continue.\n", history_[len - 1]);
	refresh();
	getch();
}

void AudioPlayer::display_queue()
{
	printw("--> [");
	int len = queue_.size();
	for(int i = 0; i < len - 1; ++i)
		printw("%d, ", queue_[i]);
	if(len > 0) printw("%d", queue_[len - 1]);
	printw("]\nPress any key to continue.\n");
	refresh();
	getch();
}

void AudioPlayer::display_duration()
{
	printw("Duration: %d:%d\nPress any key to continue.\n", duration_ / 60, duration_ % 60);
	refresh();
	getch();
}

/*_    _  ______  _       _____   ______  _____    _____ 
 | |  | ||  ____|| |     |  __ \ |  ____||  __ \  / ____|
 | |__| || |__   | |     | |__) || |__   | |__) || (___  
 |  __  ||  __|  | |     |  ___/ |  __|  |  _  /  \___ \ 
 | |  | || |____ | |____ | |     | |____ | | \ \  ____) |
 |_|  |_||______||______||_|     |______||_|  \_\|_____/*/

string AudioPlayer::get_file_ext(const string &fn)
{
	size_t ext_pos = fn.rfind('.'); //find last .

	if(ext_pos != std::string::npos)
		return to_lower(fn.substr(ext_pos + 1));
	else
		return "";
}

string AudioPlayer::to_lower(const string &str)
{
	string result = "";
	for(int i = 0, l = str.length(); i < l; ++i)
		result += tolower(str[i]);
	return result;
}

bool AudioPlayer::isnum(const string &str)
{
	for(int i = 0, l = str.length(); i < l; ++i)
		if(!isdigit(str[i])) return false;
	return true;
}

void AudioPlayer::die(string msg, int err)
{
	endwin();
	puts(msg.c_str());
	exit(err);
}

/*__  __            _____  _   _ 
 |  \/  |    /\    |_   _|| \ | |
 | \  / |   /  \     | |  |  \| |
 | |\/| |  / /\ \    | |  | . ` |
 | |  | | / ____ \  _| |_ | |\  |
 |_|  |_|/_/    \_\|_____||_| \_|*/

int main(int argc, char **argv)
{
	if(argc > 2)
	{
		printf("Usage: %s [directory]\n", argv[0]);
		exit(1);
	}

	initscr();
	cbreak(); raw(); noecho(); curs_set(0);

	string song_directory = (argc == 2) ? argv[1] : ".";
	AudioPlayer winona(song_directory);

	bool exit = false;
	while(!exit)
	{
		clear();
		winona.display_menu();
		refresh();
		exit = winona.await_command();

		if(FRAMES_READ == 0) /* song ended */
			winona.play_song(); /* so on to the next one :) */
	}
	clear();
	endwin();
	return 0;
}
