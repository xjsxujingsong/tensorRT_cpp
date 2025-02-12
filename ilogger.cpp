
#if defined(_WIN32)
#	define U_OS_WINDOWS
#else
#   define U_OS_LINUX
#endif

#include "ilogger.hpp"
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <string>
#include <mutex>
#include <memory>
#include <vector>
#include <thread>
#include <atomic>
#include <fstream>
#include <stack>

#if defined(U_OS_WINDOWS)
#	define HAS_UUID
#	include <Windows.h>
#   include <wingdi.h>
#	include <Shlwapi.h>
#	pragma comment(lib, "shlwapi.lib")
#   pragma comment(lib, "ole32.lib")
#   pragma comment(lib, "gdi32.lib")
#	undef min
#	undef max
#endif

#if defined(U_OS_LINUX)
//#	include <sys/io.h>
#	include <dirent.h>
#	include <sys/types.h>
#	include <sys/stat.h>
#	include <unistd.h>
#   include <stdarg.h>
#	define strtok_s  strtok_r
#endif


#if defined(U_OS_LINUX)
#define __GetTimeBlock						\
	time_t timep;							\
	time(&timep);							\
	tm& t = *(tm*)localtime(&timep);
#endif

#if defined(U_OS_WINDOWS)
#define __GetTimeBlock						\
	tm t;									\
	_getsystime(&t);
#endif

namespace iLogger{

    using namespace std;

    const char* level_string(int level){
		switch (level){
		case ILOGGER_VERBOSE: return "verbo";
		case ILOGGER_INFO: return "info";
		case ILOGGER_WARNING: return "warn";
		case ILOGGER_ERROR: return "error";
		case ILOGGER_FATAL: return "fatal";
		default: return "unknow";
		}
	}

	std::tuple<uint8_t, uint8_t, uint8_t> hsv2rgb(float h, float s, float v){
		const int h_i = static_cast<int>(h * 6);
		const float f = h * 6 - h_i;
		const float p = v * (1 - s);
		const float q = v * (1 - f*s);
		const float t = v * (1 - (1 - f) * s);
		float r, g, b;
		switch (h_i) {
		case 0:r = v; g = t; b = p;break;
		case 1:r = q; g = v; b = p;break;
		case 2:r = p; g = v; b = t;break;
		case 3:r = p; g = q; b = v;break;
		case 4:r = t; g = p; b = v;break;
		case 5:r = v; g = p; b = q;break;
		default:r = 1; g = 1; b = 1;break;}
		return make_tuple(static_cast<uint8_t>(r * 255), static_cast<uint8_t>(g * 255), static_cast<uint8_t>(b * 255));
	}

	std::tuple<uint8_t, uint8_t, uint8_t> random_color(int id){
		float h_plane = ((((unsigned int)id << 2) ^ 0x937151) % 100) / 100.0f;;
		float s_plane = ((((unsigned int)id << 3) ^ 0x315793) % 100) / 100.0f;
		return hsv2rgb(h_plane, s_plane, 1);
	}

    string date_now() {
		char time_string[20];
		__GetTimeBlock;

		sprintf(time_string, "%04d-%02d-%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
		return time_string;
	}

    string time_now(){
		char time_string[20];
		__GetTimeBlock;

		sprintf(time_string, "%04d-%02d-%02d %02d:%02d:%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
		return time_string;
	}

    bool mkdir(const string& path){
#ifdef U_OS_WINDOWS
		return CreateDirectoryA(path.c_str(), nullptr);
#else
		return ::mkdir(path.c_str(), 0755) == 0;
#endif
	}

    bool mkdirs(const string& path){

		if (path.empty()) return false;
		if (exists(path)) return true;

		string _path = path;
		char* dir_ptr = (char*)_path.c_str();
		char* iter_ptr = dir_ptr;
		
		bool keep_going = *iter_ptr != 0;
		while (keep_going){

			if (*iter_ptr == 0)
				keep_going = false;

#ifdef U_OS_WINDOWS
			if (*iter_ptr == '/' || *iter_ptr == '\\' || *iter_ptr == 0){
#else
			if ((*iter_ptr == '/' && iter_ptr != dir_ptr) || *iter_ptr == 0){
#endif
				char old = *iter_ptr;
				*iter_ptr = 0;
				if (!exists(dir_ptr)){
					if (!mkdir(dir_ptr)){
						if(!exists(dir_ptr)){
							INFOE("mkdirs %s return false.", dir_ptr);
							return false;
						}
					}
				}
				*iter_ptr = old;
			}
			iter_ptr++;
		}
		return true;
	}

    FILE* fopen_mkdirs(const string& path, const string& mode){

		FILE* f = fopen(path.c_str(), mode.c_str());
		if (f) return f;

		int p = path.rfind('/');

#if defined(U_OS_WINDOWS)
		int e = path.rfind('\\');
		p = std::max(p, e);
#endif
		if (p == -1)
			return nullptr;
		
		string directory = path.substr(0, p);
		if (!mkdirs(directory))
			return nullptr;

		return fopen(path.c_str(), mode.c_str());
	}

    bool exists(const string& path){

#ifdef U_OS_WINDOWS
		return ::PathFileExistsA(path.c_str());
#elif defined(U_OS_LINUX)
		return access(path.c_str(), R_OK) == 0;
#endif
	}

    string format(const char* fmt, ...) {
		va_list vl;
		va_start(vl, fmt);
		char buffer[2048];
		vsnprintf(buffer, sizeof(buffer), fmt, vl);
		return buffer;
	}

    string file_name(const string& path, bool include_suffix){

		if (path.empty()) return "";

		int p = path.rfind('/');

#ifdef U_OS_WINDOWS
		int e = path.rfind('\\');
		p = max(p, e);
#endif
		p += 1;

		//include suffix
		if (include_suffix)
			return path.substr(p);

		int u = path.rfind('.');
		if (u == -1)
			return path.substr(p);

		if (u <= p) u = path.size();
		return path.substr(p, u - p);
	}

    long long timestamp_now() {
		return chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
	}

    static struct Logger{
		mutex logger_lock_;
		string logger_directory;
		int logger_level{ILOGGER_INFO};
		vector<string> cache_, local_;
		shared_ptr<thread> flush_thread_;
		atomic<bool> keep_run_{false};
		shared_ptr<FILE> handler;

		void write(const string& line) {
			lock_guard<mutex> l(logger_lock_);

			if (!keep_run_) {

				if(flush_thread_) 
					return;

				cache_.reserve(1000);
				keep_run_ = true;
				flush_thread_.reset(new thread(std::bind(&Logger::flush_job, this)));
			}
			cache_.emplace_back(line);
		}

		void flush() {

			if (cache_.empty())
				return;

			{
				std::lock_guard<mutex> l(logger_lock_);
				std::swap(local_, cache_);
			}

			if (!local_.empty() && !logger_directory.empty()) {

				auto now = date_now();
				auto file = format("%s%s.txt", logger_directory.c_str(), now.c_str());
				if (!exists(file)) {
					handler.reset(fopen_mkdirs(file, "wb"), fclose);
				}
				else if (!handler) {
					handler.reset(fopen_mkdirs(file, "a+"), fclose);
				}

				if (handler) {
					for (auto& line : local_)
						fprintf(handler.get(), "%s\n", line.c_str());
					fflush(handler.get());
					handler.reset();
				}
			}
			local_.clear();
		}

		void flush_job() {

			auto tick_begin = timestamp_now();
			std::vector<string> local;
			while (keep_run_) {

				if (timestamp_now() - tick_begin < 1000) {
					this_thread::sleep_for(std::chrono::milliseconds(100));
					continue;
				}

				tick_begin = timestamp_now();
				flush();
			}
			flush();
		}

		void set_save_directory(const string& loggerDirectory) {
			logger_directory = loggerDirectory;

			if (logger_directory.empty())
				logger_directory = ".";

#if defined(U_OS_LINUX)
			if (logger_directory.back() != '/') {
				logger_directory.push_back('/');
			}
#endif

#if defined(U_OS_WINDOWS)
			if (logger_directory.back() != '/' && logger_directory.back() != '\\') {
				logger_directory.push_back('/');
			}
#endif
		}

		void set_logger_level(int level){
			logger_level = level;
		}

		void close(){

			if (!keep_run_) return;

			keep_run_ = false;
			flush_thread_->join();
			flush_thread_.reset();
			handler.reset();
		}

		virtual ~Logger(){
			close();
		}
	}__g_logger;

	void destroy(){
		__g_logger.close();
	}

    static void remove_color_text(char* buffer){
		
		//"\033[31m%s\033[0m"
		char* p = buffer;
		while(*p){

			if(*p == 0x1B){
				char np = *(p + 1);
				if(np == '['){
					// has token
					char* t = p + 2;
					while(*t){
						if(*t == 'm'){
							t = t + 1;
							char* k = p;
							while(*t){
								*k++ = *t++;
							}
							*k = 0;
							break;
						}
						t++;
					}
				}
			}
			p++;
		}
	}

    void set_save_directory(const string& loggerDirectory){
        __g_logger.set_save_directory(loggerDirectory);
    }

	void set_log_level(int level){
		__g_logger.set_logger_level(level);
	}

    void __log_func(const char* file, int line, int level, const char* fmt, ...) {

		if(level > __g_logger.logger_level)
			return;

        string now = time_now();
        va_list vl;
        va_start(vl, fmt);
        
        char buffer[2048];
        string filename = file_name(file, true);
        int n = snprintf(buffer, sizeof(buffer), "[%s]", now.c_str());

#if defined(U_OS_WINDOWS)
		if (level == ILOGGER_FATAL || level == ILOGGER_ERROR) {
			n += snprintf(buffer + n, sizeof(buffer) - n, "[%s]", level_string(level));
		}
		else if (level == ILOGGER_WARNING) {
			n += snprintf(buffer + n, sizeof(buffer) - n, "[%s]", level_string(level));
		}
		else {
			n += snprintf(buffer + n, sizeof(buffer) - n, "[%s]", level_string(level));
		}
#elif defined(U_OS_LINUX)
		if (level == ILOGGER_FATAL || level == ILOGGER_ERROR) {
			n += snprintf(buffer + n, sizeof(buffer) - n, "[\033[31m%s\033[0m]", level_string(level));
		}
		else if (level == ILOGGER_WARNING) {
			n += snprintf(buffer + n, sizeof(buffer) - n, "[\033[33m%s\033[0m]", level_string(level));
		}
		else {
			n += snprintf(buffer + n, sizeof(buffer) - n, "[\033[32m%s\033[0m]", level_string(level));
		}
#endif

        n += snprintf(buffer + n, sizeof(buffer) - n, "[%s:%d]:", filename.c_str(), line);
        vsnprintf(buffer + n, sizeof(buffer) - n, fmt, vl);

        if (level == ILOGGER_FATAL || level == ILOGGER_ERROR) {
            fprintf(stderr, "%s\n", buffer);
        }
        else if (level == ILOGGER_WARNING) {
            fprintf(stdout, "%s\n", buffer);
        }
        else {
            fprintf(stdout, "%s\n", buffer);
        }

#ifdef U_OS_LINUX
		// remove save color txt
		remove_color_text(buffer);
#endif 
		__g_logger.write(buffer);
		if (level == ILOGGER_FATAL) {
			__g_logger.flush();
			fflush(stdout);
			abort();
		}
    }

	std::vector<uint8_t> load_file(const string& file){

		ifstream in(file, ios::in | ios::binary);
		if (!in.is_open())
			return {};

		in.seekg(0, ios::end);
		size_t length = in.tellg();

		std::vector<uint8_t> data;
		if (length > 0){
			in.seekg(0, ios::beg);
			data.resize(length);

			in.read((char*)&data[0], length);
		}
		in.close();
		return data;
	}

	bool alphabet_equal(char a, char b, bool ignore_case){
		if (ignore_case){
			a = a > 'a' && a < 'z' ? a - 'a' + 'A' : a;
			b = b > 'a' && b < 'z' ? b - 'a' + 'A' : b;
		}
		return a == b;
	}

	static bool pattern_match_body(const char* str, const char* matcher, bool igrnoe_case){
		//   abcdefg.pnga          *.png      > false
		//   abcdefg.png           *.png      > true
		//   abcdefg.png          a?cdefg.png > true

		if (!matcher || !*matcher || !str || !*str) return false;

		const char* ptr_matcher = matcher;
		while (*str){
			if (*ptr_matcher == '?'){
				ptr_matcher++;
			}
			else if (*ptr_matcher == '*'){
				if (*(ptr_matcher + 1)){
					if (pattern_match_body(str, ptr_matcher + 1, igrnoe_case))
						return true;
				}
				else{
					return true;
				}
			}
			else if (!alphabet_equal(*ptr_matcher, *str, igrnoe_case)){
				return false;
			}
			else{
				if (*ptr_matcher)
					ptr_matcher++;
				else
					return false;
			}
			str++;
		}

		while (*ptr_matcher){
			if (*ptr_matcher != '*')
				return false;
			ptr_matcher++;
		}
		return true;
	}

	bool pattern_match(const char* str, const char* matcher, bool igrnoe_case){
		//   abcdefg.pnga          *.png      > false
		//   abcdefg.png           *.png      > true
		//   abcdefg.png          a?cdefg.png > true

		if (!matcher || !*matcher || !str || !*str) return false;

		char filter[500];
		strcpy(filter, matcher);

		vector<const char*> arr;
		char* ptr_str = filter;
		char* ptr_prev_str = ptr_str;
		while (*ptr_str){
			if (*ptr_str == ';'){
				*ptr_str = 0;
				arr.push_back(ptr_prev_str);
				ptr_prev_str = ptr_str + 1;
			}
			ptr_str++;
		}

		if (*ptr_prev_str)
			arr.push_back(ptr_prev_str);

		for (int i = 0; i < arr.size(); ++i){
			if (pattern_match_body(str, arr[i], igrnoe_case))
				return true;
		}
		return false;
	}

#ifdef U_OS_WINDOWS
	vector<string> find_files(const string& directory, const string& filter, bool findDirectory, bool includeSubDirectory){
		
		string realpath = directory;
		if (realpath.empty())
			realpath = "./";

		char backchar = realpath.back();
		if (backchar != '\\' && backchar != '/')
			realpath += "/";

		vector<string> out;
		_WIN32_FIND_DATAA find_data;
		stack<string> ps;
		ps.push(realpath);

		while (!ps.empty())
		{
			string search_path = ps.top();
			ps.pop();

			HANDLE hFind = FindFirstFileA((search_path + "*").c_str(), &find_data);
			if (hFind != INVALID_HANDLE_VALUE){
				do{
					if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0)
						continue;

					if (!findDirectory && (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != FILE_ATTRIBUTE_DIRECTORY ||
						findDirectory && (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY){
						if (PathMatchSpecA(find_data.cFileName, filter.c_str()))
							out.push_back(search_path + find_data.cFileName);
					}

					if (includeSubDirectory && (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)
						ps.push(search_path + find_data.cFileName + "/");

				} while (FindNextFileA(hFind, &find_data));
				FindClose(hFind);
			}
		}
		return out;
	}
#endif

#ifdef U_OS_LINUX
	vector<string> find_files(const string& directory, const string& filter, bool findDirectory, bool includeSubDirectory)
	{
		string realpath = directory;
		if (realpath.empty())
			realpath = "./";

		char backchar = realpath.back();
		if (backchar != '\\' && backchar != '/')
			realpath += "/";

		struct dirent* fileinfo;
		DIR* handle;
		stack<string> ps;
		vector<string> out;
		ps.push(realpath);

		while (!ps.empty())
		{
			string search_path = ps.top();
			ps.pop();

			handle = opendir(search_path.c_str());
			if (handle != 0)
			{
				while (fileinfo = readdir(handle))
				{
					struct stat file_stat;
					if (strcmp(fileinfo->d_name, ".") == 0 || strcmp(fileinfo->d_name, "..") == 0)
						continue;

					if (lstat((search_path + fileinfo->d_name).c_str(), &file_stat) < 0)
						continue;

					if (!findDirectory && !S_ISDIR(file_stat.st_mode) ||
						findDirectory && S_ISDIR(file_stat.st_mode))
					{
						if (pattern_match(fileinfo->d_name, filter.c_str()))
							out.push_back(search_path + fileinfo->d_name);
					}

					if (includeSubDirectory && S_ISDIR(file_stat.st_mode))
						ps.push(search_path + fileinfo->d_name + "/");
				}
				closedir(handle);
			}
		}
		return out;
	}
#endif

	string align_blank(const string& input, int align_size, char blank){
		if(input.size() >= align_size) return input;
		string output = input;
		for(int i = 0; i < align_size - input.size(); ++i)
			output.push_back(blank);
		return output;
	}

	bool save_file(const string& file, const void* data, size_t length, bool mk_dirs){

		if (mk_dirs){
			int p = (int)file.rfind('/');

#ifdef U_OS_WINDOWS
			int e = (int)file.rfind('\\');
			p = max(p, e);
#endif
			if (p != -1){
				if (!mkdirs(file.substr(0, p)))
					return false;
			}
		}

		FILE* f = fopen(file.c_str(), "wb");
		if (!f) return false;

		if (data && length > 0){
			if (fwrite(data, 1, length, f) != length){
				fclose(f);
				return false;
			}
		}
		fclose(f);
		return true;
	}

	bool save_file(const string& file, const vector<uint8_t>& data, bool mk_dirs){
		return save_file(file, data.data(), data.size(), mk_dirs);
	}
}; // namespace Logger